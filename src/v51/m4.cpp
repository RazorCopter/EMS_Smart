// SPDX-License-Identifier: MIT
#include "m4.h"
#include "live_trial.h"
#include "transport/v51_read_only.h"
#include "clock_service.h"
#include "ems_runtime.h"
#include "m5.h"
#include "remote.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <driver/gpio.h>
#include <esp_crt_bundle.h>
#include <esp_wifi.h>
#include <memory>
#include <mqtt_client.h>
#include <mutex>
#include <sstream>

#ifndef V51_BUILD_ID
#define V51_BUILD_ID "v5.1-remote-1"
#endif

namespace heliograph::v51 {
namespace {
struct Channel {
  const char *key;
  const char *name;
  const char *unit;
  const char *type;
  Sample EnergyState::*sample;
};
constexpr Channel channels[] = {
    {"soc", "Battery SOC", "%", "battery", &EnergyState::soc},
    {"battery_w", "Battery power", "W", "power", &EnergyState::battery},
    {"battery_v", "Battery voltage", "V", "voltage",
     &EnergyState::batteryVoltage},
    {"battery_a", "Battery current", "A", "current",
     &EnergyState::batteryCurrent},
    {"battery_temp_c", "Battery temperature", "°C", "temperature",
     &EnergyState::batteryTemp},
    {"soh_pct", "Battery SOH", "%", "battery", &EnergyState::soh},
    {"pv_w", "PV total", "W", "power", &EnergyState::pv},
    {"pv1_v", "PV1 voltage", "V", "voltage", &EnergyState::pv1v},
    {"pv1_a", "PV1 current", "A", "current", &EnergyState::pv1a},
    {"pv1_w", "PV1 power", "W", "power", &EnergyState::pv1w},
    {"pv2_v", "PV2 voltage", "V", "voltage", &EnergyState::pv2v},
    {"pv2_a", "PV2 current", "A", "current", &EnergyState::pv2a},
    {"pv2_w", "PV2 power", "W", "power", &EnergyState::pv2w},
    {"grid_w", "Grid power", "W", "power", &EnergyState::grid},
    {"grid_v", "Grid voltage", "V", "voltage", &EnergyState::voltage},
    {"hz", "Grid frequency", "Hz", "frequency", &EnergyState::frequency},
    {"load_w", "Derived load", "W", "power", &EnergyState::load},
    {"inverter_w", "Inverter AC power", "W", "power", &EnergyState::inverter},
    {"temp_c", "Inverter heatsink", "°C", "temperature",
     &EnergyState::temperature},
};
struct Broker {
  bool enabled = false, tls = false, retain = true;
  int port = 1883, qos = 0, telemetry_s = 5, diagnostics_s = 30, keepalive = 30,
      timeout_ms = 3000;
  char host[96]{}, user[64]{}, password[96]{}, device[32] = "afore-ems",
                                               base[96] = "afore/afore-ems/v1",
                                               prefix[32] = "homeassistant";
};
std::mutex stateLock, brokerLock, eventLock;
ClockStamp linkedClock{}, publishedClock{}, commandClock{};
std::atomic<bool> connected{false};
std::atomic<bool> availabilityPublished{false};
std::atomic<int> inputSubMsgId{-1};
std::atomic<uint32_t> linkedAt{0}, publishedAt{0}, reconnectCount{0};
std::atomic<bool> connectedOnce{false};
std::atomic<int> lastError{0};
Preferences *nvs = nullptr;
Authorize authorized = nullptr;
Broker configured, active;
esp_mqtt_client_handle_t client = nullptr;
uint32_t brokerRevision = 1;
std::string lastAck, lastCommand;
std::array<std::string, 32> ids;
size_t idIndex = 0;
constexpr size_t records = 256, recordSize = 2048;
char *history = nullptr;
uint32_t cursor = 0;
EnergyState snapshot;
uint32_t sampleAt = 0;
std::string latest;

std::string encode(JsonDocument &doc) {
  std::string json;
  serializeJson(doc, json);
  return json;
}
esp_err_t jsonResponse(httpd_req_t *req, std::string value) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, value.data(), value.size());
}
void telemetry(JsonDocument &d, const EnergyState &e, uint32_t now) {
  JsonObject freshness = d["quality"].to<JsonObject>();
  for (auto &c : channels) {
    const auto &sample = e.*c.sample;
    auto value = sample.usable(now);
    if (value)
      d[c.key] = *value;
    else
      d[c.key] = nullptr;
    JsonObject q = freshness[c.key].to<JsonObject>();
    auto age = sample.age(now);
    if (age)
      q["age_ms"] = *age;
    else
      q["age_ms"] = nullptr;
    q["source"] = c.sample == &EnergyState::load ? "derived" : "measured";
    q["state"] = value                                   ? "valid"
                 : sample.quality(now) == Quality::Stale ? "stale"
                                                         : "unavailable";
  }
  d["reads"] = e.goodReads;
  d["errors"] = e.errors;
  d["fault"] = e.readHalted;
  d["grid_batt_skew_ms"] = nullptr;
  if (e.grid.usable(now) && e.battery.usable(now))
    d["grid_batt_skew_ms"] = std::min(e.grid.stamp - e.battery.stamp,
                                      e.battery.stamp - e.grid.stamp);
  d["telemetry_valid"] = !e.readHalted && e.grid.usable(now).has_value() &&
                         e.soc.usable(now).has_value() &&
                         e.battery.usable(now).has_value();
  d["shadow"] = !liveTrialActive();
  d["aux1"] = auxOn() ? "ON" : "OFF";
  d["modbus_write_count"] = modbusWriteCount();
  d["mqtt"] = connected.load();
  auto stamp = clockStamp();

  stamp.uptime = now;
  addClock(d.as<JsonObject>(), stamp);
}
bool publish(const char *suffix, const std::string &value,
             bool retain = false) {
  const auto topic = std::string(active.base) + "/" + suffix;
  const bool ok =
      esp_mqtt_client_enqueue(client, topic.c_str(), value.c_str(),
                              value.size(), active.qos, retain, true) >= 0;
  if (ok) {
    const auto stamp = clockStamp();
    const std::lock_guard guard(eventLock);
    publishedClock = stamp;
    publishedAt = stamp.uptime;
  }
  return ok;
}

void discovery() {
  const std::string base = active.base;
  for (auto &c : channels) {
    JsonDocument d;
    const std::string id = std::string(active.device) + "_" + c.key;
    d["name"] = c.name;
    d["unique_id"] = id;
    d["device"]["identifiers"][0] = active.device;
    d["device"]["name"] = "Afore Autonomous EMS";
    d["device"]["manufacturer"] = "Afore EMS";
    d["state_topic"] = base + "/telemetry";
    d["availability_topic"] = base + "/availability";
    d["unit_of_measurement"] = c.unit;
    d["device_class"] = c.type;
    d["state_class"] = "measurement";
    d["value_template"] = "{{ value_json." + std::string(c.key) + " }}";
    auto topic = std::string(active.prefix) + "/sensor/" + id + "/config";
    auto data = encode(d);
    esp_mqtt_client_enqueue(client, topic.c_str(), data.c_str(), data.size(), 1,
                            1, true);
  }
  struct Entity {
    const char *component;
    const char *key;
    const char *label;
    const char *path;
    int max;
  };
  const Entity entities[] = {
      {"binary_sensor", "telemetry_valid", "Telemetry valid", "telemetry", 0},
      {"binary_sensor", "mqtt", "MQTT", "telemetry", 0},
      {"binary_sensor", "shadow", "SHADOW", "telemetry", 0},
      {"binary_sensor", "fault", "Bus fault", "telemetry", 0},
      {"select", "operating_mode", "Operating mode", "system/manual", 0},
      {"switch", "aux1", "AUX1", "system/manual", 1},
      {"sensor", "live_lock", "LIVE lock reason", "system/manual", 0},
      {"sensor", "capacity_wh", "Battery useful capacity", "config/state", 0},
      {"sensor", "nominal_mv", "Battery nominal voltage", "config/state", 0},
      {"select", "strategy", "Local strategy", "config/state", 0},
      {"sensor", "strategy", "Effective strategy", "ems/state", 0},
      {"sensor", "simulated_w", "Simulated power", "ems/decision", 0},
      {"number", "soc_min", "Minimum SOC", "config/state", 100},
      {"number", "soc_max", "Maximum SOC", "config/state", 100},
      {"number", "soc_target", "Target SOC", "config/state", 100},
      {"number", "charge_w", "Charge power limit", "config/state", 20000},
      {"number", "discharge_w", "Discharge power limit", "config/state", 20000},
      {"number", "charge_a", "Charge current limit", "config/state", 500},
      {"number", "discharge_a", "Discharge current limit", "config/state", 500},
      {"number", "voltage_min", "Minimum battery voltage", "config/state",
       1000},
      {"number", "voltage_max", "Maximum battery voltage", "config/state",
       1000},
      {"switch", "grid_charge", "Allow simulated grid charge", "config/state",
       1},
      {"sensor", "candidate_w", "Candidate power", "ems/state", 0},
      {"sensor", "limited_w", "Safety-limited power", "ems/state", 0},
      {"sensor", "reason", "Decision reason", "ems/state", 0},
      {"sensor", "safety_reason", "Safety reason", "ems/state", 0},
      {"sensor", "state", "Strategy state", "ems/state", 0},
      {"sensor", "timestamp", "Last decision", "ems/state", 0},
      {"binary_sensor", "ha_online", "Home Assistant online",
       "system/ha_inputs", 0},
      {"binary_sensor", "time_valid", "Time synchronized", "diagnostics", 0},
      {"sensor", "rssi", "Wi-Fi RSSI", "diagnostics", 0},
      {"sensor", "uptime_ms", "Uptime", "diagnostics", 0},
      {"sensor", "reads", "FC04 reads", "diagnostics", 0},
      {"sensor", "errors", "Modbus read errors", "diagnostics", 0},
      {"sensor", "firmware", "Firmware", "diagnostics", 0},
      {"sensor", "modbus_write_count", "Modbus writes", "diagnostics", 0},
      {"sensor", "aux_auto_off_ms", "AUX1 auto-off", "system/manual", 0},
      {"sensor", "last_aux_origin", "Last AUX1 origin", "system/manual", 0},
      {"sensor", "deadline_minute", "Target SOC deadline", "config/state", 0},
      {"number", "target_deadline_s", "Relative target deadline",
       "config/state", 604800},
      {"number", "charge_first_s", "Charge First duration limit",
       "config/state", 604800},
      {"switch", "predictive", "Predictive EMS", "config/state", 1},
      {"switch", "auto_season", "Auto seasonal SOC", "config/state", 1},
      {"number", "summer_soc_min", "Summer min SOC", "config/state", 99},
      {"number", "winter_soc_min", "Winter min SOC", "config/state", 99},
      {"number", "low_sun_kwh", "Low sun threshold", "config/state", 100},
      {"number", "weekday_load_w", "Weekday average load", "config/state",
       20000},
      {"number", "weekend_load_w", "Weekend average load", "config/state",
       20000},
      {"sensor", "recommended_charge_a", "Recommended charge current",
       "ems/state", 0},
      {"sensor", "effective_soc_min", "Effective min SOC", "ems/state", 0},
      {"sensor", "predictive_phase", "Predictive phase", "ems/state", 0},
      {"sensor", "season", "EMS season", "ems/state", 0},
      {"binary_sensor", "fog_active", "Fog protection active", "ems/state", 0},
      {"binary_sensor", "low_sun_mode", "Low sun mode", "ems/state", 0},
      {"button", "target_soc", "Start Target SOC", "config/state", 0},
      {"button", "maintenance", "Start Maintenance", "config/state", 0},
      {"button", "cancel_override", "Cancel override", "config/state", 0},
      {"button", "cancel_session", "Cancel session", "config/state", 0},
  };
  for (const auto &e : entities) {
    JsonDocument d;
    std::string component = e.component, key = e.key,
                id = std::string(active.device) + "_" + component + "_" + key;
    d["name"] = e.label;
    d["unique_id"] = id;
    d["device"]["identifiers"][0] = active.device;
    d["device"]["name"] = "Afore Autonomous EMS";
    d["availability_topic"] = base + "/availability";
    if (component != "button")
      d["state_topic"] = base + "/" + e.path;
    if (component == "binary_sensor")
      d["value_template"] = "{{ 'ON' if value_json." + key + " else 'OFF' }}";
    else if (component == "select") {
      d["value_template"] = "{{ value_json.strategy_name }}";
      if (key == "operating_mode") {
        d["value_template"] = "{{ value_json.operating_mode }}";
        d["options"].add("SHADOW");
        d["options"].add("LIVE");
      } else
        for (auto name : {"SELF_USE", "CHARGE_FIRST", "NATIVE_INVERTER",
                          "SAFE_MODE", "TARGET_SOC", "MAINTENANCE"})
          d["options"].add(name);
    } else if (component == "switch") {
      d["value_template"] =
          key == "aux1" ? "{{ value_json.aux1 }}"
                        : "{{ 'ON' if value_json." + key + " else 'OFF' }}";
    } else if (component != "button")
      d["value_template"] = "{{ value_json." + key + " }}";
    if (component == "number" || component == "select" ||
        (component == "switch" && key != "aux1"))
      d["entity_category"] = "config";
    if (std::string(e.path) == "diagnostics")
      d["entity_category"] = "diagnostic";
    if (key == "time_valid")
      d["value_template"] =
          "{{ 'ON' if value_json.time_source == 'NTP' else 'OFF' }}";
    if (key == "timestamp")
      d["device_class"] = "timestamp";
    if (key == "rssi")
      d["device_class"] = "signal_strength", d["unit_of_measurement"] = "dBm";
    if (key == "uptime_ms" || key == "aux_auto_off_ms") {
      d["unit_of_measurement"] = "s";
      d["device_class"] = "duration";
      d["value_template"] = "{{ (value_json." + key + " / 1000) | int }}";
    }
    if (key == "deadline_minute")
      d["value_template"] =
          "{{ '%02d:%02d' | format(value_json.deadline_minute // 60, "
          "value_json.deadline_minute % 60) }}";
    if (key == "target_deadline_s" || key == "charge_first_s")
      d["unit_of_measurement"] = "s";
    if (key == "capacity_wh")
      d["unit_of_measurement"] = "Wh";
    if (key == "low_sun_kwh")
      d["unit_of_measurement"] = "kWh";
    if (key == "nominal_mv") {
      d["unit_of_measurement"] = "V";
      d["value_template"] = "{{ value_json.nominal_mv / 1000 }}";
    }
    if (key == "candidate_w" || key == "limited_w" || key == "simulated_w" ||
        key == "charge_w" || key == "discharge_w" || key == "weekday_load_w" ||
        key == "weekend_load_w")
      d["unit_of_measurement"] = "W";
    if (key == "charge_a" || key == "discharge_a" ||
        key == "recommended_charge_a")
      d["unit_of_measurement"] = "A";
    if (key == "voltage_min" || key == "voltage_max")
      d["unit_of_measurement"] = "V";
    if (key.rfind("soc_", 0) == 0 || key == "summer_soc_min" ||
        key == "winter_soc_min" || key == "effective_soc_min")
      d["unit_of_measurement"] = "%";
    if (component == "number") {
      d["min"] = (key == "soc_max" || key == "soc_target" ||
                  key == "target_deadline_s" || key == "charge_first_s")
                     ? 1
                     : 0;
      d["max"] = key == "soc_min" ? 99 : e.max;
      d["step"] = 1;
    }
    const auto envelope =
        "{\"schema\":1,\"message_id\":\"" + id +
        "-{{ (now().timestamp()*1000000)|int }}\",\"timestamp\":{{ "
        "now().timestamp()|int }},\"ttl\":30,";
    if (component == "button") {
      d["command_topic"] = base + "/ems/command";
      d["command_template"] =
          envelope + "\"action\":\"" + key + "\",\"payload\":{}}";
    } else if (component == "number" || component == "select" ||
               component == "switch") {
      const std::string value =
          component == "select"
              ? "{{ "
                "['SELF_USE','CHARGE_FIRST','NATIVE_INVERTER','SAFE_MODE','"
                "TARGET_SOC','MAINTENANCE'].index(value) }}"
          : component == "switch" ? "1"
                                  : "{{ value|int }}";
      auto tmpl = [&](const std::string &v) {
        return envelope + "\"action\":\"policy\",\"payload\":{\"" + key +
               "\":" + v + "}}";
      };
      d["command_topic"] = base + "/ems/command";
      if (key == "operating_mode")
        d["command_template"] =
            envelope + "\"action\":\"operating_mode\",\"payload\":{\"mode\":\"{"
                       "{ value }}\"}}";
      else if (key == "aux1")
        d["command_template"] =
            envelope +
            "\"action\":\"aux\",\"payload\":{\"on\":{{ 'true' if value=='ON' "
            "else 'false' }},\"auto_off_s\":{{ 30 if value=='ON' else 0 }}}}";
      else if (component == "switch") {
        d["command_template"] = tmpl("{{ 1 if value=='ON' else 0 }}");
      } else
        d["command_template"] = tmpl(value);
    }
    auto topic =
        std::string(active.prefix) + "/" + component + "/" + id + "/config";
    auto data = encode(d);
    esp_mqtt_client_enqueue(client, topic.c_str(), data.c_str(), data.size(), 1,
                            1, true);
  }
}
void handleEvent(void *, esp_event_base_t, int32_t code, void *data) {
  auto *e = static_cast<esp_mqtt_event_handle_t>(data);
  if (code == MQTT_EVENT_CONNECTED) {
    if (connectedOnce.exchange(true))
      reconnectCount.fetch_add(1);
    connected = true;
    availabilityPublished = false;
    inputSubMsgId = -1;
    {
      const auto stamp = clockStamp();
      const std::lock_guard guard(eventLock);
      linkedClock = stamp;
      linkedAt = stamp.uptime;
    }
    const auto topic = std::string(active.base) + "/ems/command";
    esp_mqtt_client_subscribe(e->client, topic.c_str(), active.qos);
    const auto solar = std::string(active.base) + "/forecast/solar";
    esp_mqtt_client_subscribe(e->client, solar.c_str(), active.qos);
    const auto inputs = std::string(active.base) + "/input/+";
    inputSubMsgId =
        esp_mqtt_client_subscribe(e->client, inputs.c_str(), active.qos);
  } else if (code == MQTT_EVENT_SUBSCRIBED) {
    const int expected = inputSubMsgId.load();
    if ((e->msg_id == expected || expected < 0) &&
        !availabilityPublished.exchange(true)) {
      publish("availability", "online", true);
    }
  } else if (code == MQTT_EVENT_DISCONNECTED) {
    connected = false;
    availabilityPublished = false;
    inputSubMsgId = -1;
  } else if (code == MQTT_EVENT_ERROR)
    lastError = e->error_handle ? e->error_handle->error_type : -1;
  else if (code == MQTT_EVENT_DATA && e->topic &&
           e->data_len == e->total_data_len && e->current_data_offset == 0 &&
           std::string(e->topic, e->topic_len) !=
               std::string(active.base) + "/ems/command") {
    const auto name = std::string(e->topic, e->topic_len);
    const auto root = std::string(active.base) + "/input/";
    const char *kind = name.compare(0, root.size(), root) == 0
                           ? name.c_str() + root.size()
                       : name == std::string(active.base) + "/forecast/solar"
                           ? "forecast_solar"
                           : nullptr;
    if (kind) {
      if (e->retain)
        rejectRetainedHaInput(kind);
      else
        acceptHaInput(kind, e->data, e->data_len);
    }
  } else if (code == MQTT_EVENT_DATA && e->topic && e->data_len <= 768 &&
             e->data_len == e->total_data_len && e->current_data_offset == 0 &&
             std::string(e->topic, e->topic_len) ==
                 std::string(active.base) + "/ems/command") {
    JsonDocument d;
    if (deserializeJson(d, e->data, e->data_len))
      return;
    const std::string id = d["message_id"] | "";
    const std::string action = d["action"] | "policy";
    const int64_t timestamp = d["timestamp"] | int64_t(0);
    const int ttl = d["ttl"] | 0;
    const auto arrival = clockStamp();
    const int64_t now = arrival.utc;
    const char *reason = "INVALID";
    bool ok = false;
    if (e->retain)
      reason = "RETAINED";
    else if (!now)
      reason = "CLOCK_UNAVAILABLE";
    else if (ttl < 1 || ttl > 600 || timestamp <= 0 || timestamp > now + 5 ||
             timestamp <= now - ttl)
      reason = "EXPIRED";
    else if (d["schema"] == 1 && !id.empty() && id.size() <= 80) {
      if (std::find(ids.begin(), ids.end(), id) != ids.end())
        reason = "DUPLICATE";
      else {
        ids[idIndex++ % ids.size()] = id;
        std::string form;
        bool allInts = d["payload"].is<JsonObject>();
        for (auto field : d["payload"].as<JsonObject>()) {
          if (!field.value().is<int>() ||
              strspn(field.key().c_str(),
                     "abcdefghijklmnopqrstuvwxyz_0123456789") !=
                  strlen(field.key().c_str())) {
            allInts = false;
            break;
          }
          if (!form.empty())
            form += '&';
          form += field.key().c_str();
          form += '=';
          form += std::to_string(field.value().as<int>());
        }
        if (action == "aux" || action == "operating_mode") {
          const auto result =
              manualCommand(action.c_str(), d["payload"], "MQTT");
          ok = result.accepted;
          reason = result.reason;
        } else if (action == "cancel_override" || action == "cancel_session")
          ok = applyPolicy({}, 0, action == "cancel_session" ? 2 : 1);
        else if (allInts &&
                 (action == "policy" || action == "override" ||
                  action == "target_soc" || action == "maintenance")) {
          if (action == "target_soc" || action == "maintenance") {
            if (!form.empty())
              form += '&';
            form += action == "target_soc" ? "strategy=4" : "strategy=5";
          }
          ok = applyPolicy(
              form, action == "override"
                        ? 1000u * std::min<int64_t>(ttl, timestamp + ttl - now)
                        : 0);
        }
        if (action != "aux" && action != "operating_mode")
          reason = ok ? "ACCEPTED" : "POLICY_REJECTED";
      }
    }
    JsonDocument ack;
    const auto info = policyStatus();
    ack["configuration_revision"] = info.revision;
    const auto requested = d["payload"]["strategy"];
    if (action == "target_soc")
      ack["requested_strategy"] = "TARGET_SOC";
    else if (action == "maintenance")
      ack["requested_strategy"] = "MAINTENANCE";
    else if (requested.is<int>() && requested.as<int>() >= 0 &&
             requested.as<int>() < 6)
      ack["requested_strategy"] =
          strategyName(static_cast<Strategy>(requested.as<int>()));
    else
      ack["requested_strategy"] = nullptr;
    std::string confirmed;
    {
      const std::lock_guard guard(stateLock);
      confirmed = latest;
    }
    JsonDocument decision;
    deserializeJson(decision, confirmed);
    ack["effective_strategy"] = decision["strategy"];
    ack["configured_strategy"] = strategyName(info.configured);
    ack["schema"] = 1;
    ack["message_id"] = id;
    ack["accepted"] = ok;
    ack["reason"] = reason;
    ack["executed"] = false;
    if ((action == "aux" || action == "operating_mode") &&
        (strcmp(reason, "RETAINED") == 0 || strcmp(reason, "EXPIRED") == 0 ||
         strcmp(reason, "DUPLICATE") == 0 ||
         strcmp(reason, "CLOCK_UNAVAILABLE") == 0 ||
         strcmp(reason, "INVALID") == 0)) {
      char row[128];
      snprintf(row, sizeof(row), "MANUAL origin=MQTT action=%s outcome=%s",
               action.c_str(), reason);
      recordLog(row);
    }
    ack["aux1"] = auxOn() ? "ON" : "OFF";
    ack["physical_feedback"] = nullptr;
    addClock(ack.as<JsonObject>(), arrival);
    auto value = encode(ack);
    {
      const std::lock_guard guard(brokerLock);
      lastAck = value;
      lastCommand = action;
      commandClock = arrival;
    }
    publish("ems/ack", value);
  }
}
void brokerTask(void *) {
  uint32_t revision = 0, lastState = 0, lastInfo = 0, seenConnection = 0;
  std::string willTopic;
  while (true) {
    Broker desired;
    uint32_t next;
    {
      const std::lock_guard guard(brokerLock);
      desired = configured;
      next = brokerRevision;
    }
    if (revision != next) {
      if (client) {
        if (connected)
          publish("availability", "offline", true);
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = nullptr;
      }
      connected = false;
      availabilityPublished = false;
      inputSubMsgId = -1;
      active = desired;
      revision = next;
      seenConnection = 0;
    }
    if (!client && active.enabled && WiFi.status() == WL_CONNECTED) {
      willTopic = std::string(active.base) + "/availability";
      esp_mqtt_client_config_t c{};
      c.broker.address.hostname = active.host;
      c.broker.address.port = active.port;
      c.broker.address.transport =
          active.tls ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
      if (active.tls)
        c.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
      c.credentials.client_id = active.device;
      c.credentials.username = active.user;
      c.credentials.authentication.password = active.password;
      c.session.keepalive = active.keepalive;
      c.network.timeout_ms = active.timeout_ms;
      c.session.last_will.topic = willTopic.c_str();
      c.session.last_will.msg = "offline";
      c.session.last_will.retain = 1;
      c.session.last_will.qos = 1;
      c.outbox.limit = 32768;
      c.buffer.size = 8192;
      c.task.stack_size = 8192;
      client = esp_mqtt_client_init(&c);
      if (client) {
        esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, handleEvent,
                                       nullptr);
        if (esp_mqtt_client_start(client) != ESP_OK) {
          esp_mqtt_client_destroy(client);
          client = nullptr;
          lastError = -2;
        }
      }
    }
    if (connected) {
      uint32_t now = millis();
      if (!availabilityPublished.load() && (now - linkedAt) >= 2000) {
        if (!availabilityPublished.exchange(true))
          publish("availability", "online", true);
      }
      if (seenConnection != linkedAt) {
        seenConnection = linkedAt;
        discovery();
        lastState = now - active.telemetry_s * 1000;
        lastInfo = now - active.diagnostics_s * 1000;
      }
      if (now - lastState >= 1000u * active.telemetry_s) {
        lastState = now;
        EnergyState e;
        std::string decision;
        {
          const std::lock_guard guard(stateLock);
          e = snapshot;
          decision = latest;
        }
        JsonDocument doc;
        telemetry(doc, e, now);
        publish("telemetry", encode(doc), active.retain);
        publish("system/manual", manualState(), true);
        auto external = haSummary(false, true);
        std::ostringstream timing;
        appendClock(timing, clockStamp());
        external.pop_back();
        external += timing.str() + "}";
        publish("system/ha_inputs", external, active.retain);
        if (!decision.empty()) {
          publish("ems/state", decision, active.retain);
          publish("ems/decision", decision);
        }
      }
      if (now - lastInfo >= 1000u * active.diagnostics_s) {
        lastInfo = now;
        std::ostringstream config;
        appendPolicyJson(config);
        publish("config/state", config.str(), active.retain);
        const auto res = sampleSystemResourceSummary();
        JsonDocument doc;
        doc["rssi"] = WiFi.RSSI();
        doc["heap"] = res.heapFree;
        doc["heap_total"] = res.heapTotal;
        doc["heap_min"] = res.heapMin;
        doc["heap_max_alloc"] = res.heapMaxAlloc;
        doc["heap_used_pct"] = res.heapUsedPct;
        doc["psram"] = res.psramTotal;
        doc["psram_free"] = res.psramFree;
        doc["psram_min"] = res.psramMin;
        doc["psram_used_pct"] = res.psramUsedPct;
        if (res.cpuValid) {
          doc["cpu_pct"] = res.cpuPct;
          doc["cpu0_pct"] = res.cpu0Pct;
          doc["cpu1_pct"] = res.cpu1Pct;
        } else {
          doc["cpu_pct"] = nullptr;
          doc["cpu0_pct"] = nullptr;
          doc["cpu1_pct"] = nullptr;
        }
        doc["cpu_mhz"] = res.cpuMhz;
        doc["task_count"] = res.taskCount;
        doc["min_task_stack_free_b"] = res.minTaskStackFreeB;
        doc["min_task_stack_name"] = res.minTaskStackName;
        doc["min_application_stack_free_b"] = res.minApplicationStackFreeB;
        doc["min_application_stack_name"] = res.minApplicationStackName;
        doc["mqtt_reconnect_count"] = reconnectCount.load();
        doc["nvs_used_entries"] = res.nvsUsedEntries;
        doc["nvs_free_entries"] = res.nvsFreeEntries;
        doc["nvs_total_entries"] = res.nvsTotalEntries;
        doc["nvs_used_pct"] = res.nvsUsedPct;
        doc["flash_size"] = res.flashSize;
        doc["sketch_size"] = res.sketchSize;
        doc["sketch_free"] = res.sketchFree;
        doc["uptime_ms"] = now;
        doc["reset_reason"] = esp_reset_reason();
        doc["aux1"] = auxOn() ? "ON" : "OFF";
        doc["firmware"] = V51_BUILD_ID;
        {
          const std::lock_guard guard(stateLock);
          doc["reads"] = snapshot.goodReads;
          doc["errors"] = snapshot.errors;
        }
        doc["modbus_write_count"] = modbusWriteCount();
        addClock(doc.as<JsonObject>(), clockStamp());
        publish("diagnostics", encode(doc), active.retain);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

esp_err_t getTelemetry(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  JsonDocument doc;
  {
    const std::lock_guard guard(stateLock);
    telemetry(doc, snapshot, millis());
  }
  return jsonResponse(r, encode(doc));
}
esp_err_t getDecision(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  const std::lock_guard guard(stateLock);
  return jsonResponse(r, latest.empty() ? "null" : latest);
}
esp_err_t getDecisions(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  httpd_resp_set_type(r, "application/x-ndjson");
  std::unique_ptr<char[]> line(new char[recordSize + 1]);
  if (!line)
    return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
  uint32_t end;
  {
    const std::lock_guard guard(stateLock);
    end = cursor;
  }
  if (history)
    for (uint32_t i = end > records ? end - records : 0; i < end; ++i) {
      {
        const std::lock_guard guard(stateLock);
        if (cursor - i > records)
          continue;
        memcpy(line.get(), history + (i % records) * recordSize, recordSize);
      }
      line[recordSize] = '\0';
      if (line[0] && (httpd_resp_sendstr_chunk(r, line.get()) != ESP_OK ||
                      httpd_resp_sendstr_chunk(r, "\n") != ESP_OK))
        return ESP_FAIL;
    }
  return httpd_resp_send_chunk(r, nullptr, 0);
}
void brokerJson(JsonDocument &d) {
  const std::lock_guard guard(brokerLock);
  const auto &b = configured;
  d["enabled"] = b.enabled;
  d["tls"] = b.tls;
  d["retain"] = b.retain;
  d["port"] = b.port;
  d["qos"] = b.qos;
  d["telemetry_s"] = b.telemetry_s;
  d["diagnostics_s"] = b.diagnostics_s;
  d["keepalive"] = b.keepalive;
  d["timeout_ms"] = b.timeout_ms;
  d["host"] = b.host;
  d["user"] = b.user;
  d["device"] = b.device;
  d["base"] = b.base;
  d["prefix"] = b.prefix;
  d["password_configured"] = b.password[0] != 0;
  d["schema_version"] = 1;
  d["revision"] = brokerRevision;
  d["connected"] = connected.load();
  d["last_connect_ms"] = linkedAt.load();
  d["last_publish_ms"] = publishedAt.load();
  d["last_error"] = lastError.load();
  d["last_command"] = lastCommand;
  d["last_ack"] = lastAck;
  {
    const std::lock_guard events(eventLock);
    if (linkedClock.uptime)
      addClock(d["last_connect_time"].to<JsonObject>(), linkedClock);
    if (publishedClock.uptime)
      addClock(d["last_publish_time"].to<JsonObject>(), publishedClock);
  }
  if (!lastCommand.empty())
    addClock(d["last_command_time"].to<JsonObject>(), commandClock);
}
esp_err_t getBroker(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  JsonDocument doc;
  brokerJson(doc);
  return jsonResponse(r, encode(doc));
}
bool validBroker(const Broker &b) {
  auto simple = [](const char *s, const char *allowed) {
    return *s && strspn(s, allowed) == strlen(s);
  };
  constexpr char symbols[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
  constexpr char topic[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-/";
  return (!b.enabled || simple(b.host, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM"
                                       "NOPQRSTUVWXYZ0123456789-.:")) &&
         simple(b.device, symbols) && simple(b.base, topic) &&
         simple(b.prefix, topic) && b.port >= 1 && b.port <= 65535 &&
         b.qos >= 0 && b.qos <= 1 && b.keepalive >= 5 && b.keepalive <= 300 &&
         b.telemetry_s >= 1 && b.telemetry_s <= 300 && b.diagnostics_s >= 5 &&
         b.diagnostics_s <= 600 && b.timeout_ms >= 500 && b.timeout_ms <= 15000;
}
esp_err_t saveBroker(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  auto invalid = [&]() {
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "Invalid MQTT configuration");
  };
  char data[1024];
  if (!r->content_len || r->content_len >= sizeof(data))
    return invalid();
  size_t size = 0;
  while (size < r->content_len) {
    int n = httpd_req_recv(r, data + size, r->content_len - size);
    if (n <= 0)
      return ESP_FAIL;
    size += n;
  }
  JsonDocument doc;
  if (deserializeJson(doc, data, size) || !doc.is<JsonObject>())
    return invalid();
  Broker b;
  {
    const std::lock_guard guard(brokerLock);
    b = configured;
  }
  const bool rollback = doc.size() == 1 && doc["rollback"] == true;
  if (rollback) {
    if (nvs->getBytesLength("mqtt-prev") != sizeof(b) ||
        nvs->getBytes("mqtt-prev", &b, sizeof(b)) != sizeof(b))
      return invalid();
  } else
    for (auto field : doc.as<JsonObject>()) {
      const std::string k = field.key().c_str();
      auto v = field.value();
      if (k == "enabled" || k == "tls" || k == "retain") {
        if (!v.is<bool>())
          return invalid();
        (k == "enabled" ? b.enabled
         : k == "tls"   ? b.tls
                        : b.retain) = v.as<bool>();
        continue;
      }
      struct Int {
        const char *name;
        int Broker::*field;
      };
      const Int nums[] = {{"port", &Broker::port},
                          {"qos", &Broker::qos},
                          {"telemetry_s", &Broker::telemetry_s},
                          {"diagnostics_s", &Broker::diagnostics_s},
                          {"keepalive", &Broker::keepalive},
                          {"timeout_ms", &Broker::timeout_ms}};
      auto num = std::find_if(std::begin(nums), std::end(nums),
                              [&](auto n) { return k == n.name; });
      if (num != std::end(nums)) {
        if (!v.is<int>())
          return invalid();
        b.*(num->field) = v.as<int>();
        continue;
      }
      char *p = nullptr;
      size_t cap = 0;
      if (k == "host") {
        p = b.host;
        cap = sizeof(b.host);
      } else if (k == "user") {
        p = b.user;
        cap = sizeof(b.user);
      } else if (k == "password") {
        p = b.password;
        cap = sizeof(b.password);
      } else if (k == "device") {
        p = b.device;
        cap = sizeof(b.device);
      } else if (k == "base") {
        p = b.base;
        cap = sizeof(b.base);
      } else if (k == "prefix") {
        p = b.prefix;
        cap = sizeof(b.prefix);
      }
      if (!p || !v.is<const char *>() || strlen(v.as<const char *>()) >= cap)
        return invalid();
      strcpy(p, v.as<const char *>());
    }
  if (doc["base"].isNull() && doc["device"].is<const char *>())
    snprintf(b.base, sizeof(b.base), "afore/%s/v1", b.device);
  if (!validBroker(b))
    return invalid();
  Broker previous;
  {
    const std::lock_guard guard(brokerLock);
    previous = configured;
  }
  if ((!rollback && nvs->putBytes("mqtt-prev", &previous, sizeof(previous)) !=
                        sizeof(previous)) ||
      nvs->putBytes("mqtt-v1", &b, sizeof(b)) != sizeof(b))
    return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "MQTT persistence failed");
  {
    const std::lock_guard guard(brokerLock);
    configured = b;
    ++brokerRevision;
  }
  return getBroker(r);
}
esp_err_t testBroker(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  {
    const std::lock_guard guard(brokerLock);
    if (!configured.enabled)
      return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "MQTT disabled");
    ++brokerRevision;
  }
  return jsonResponse(r, "{\"reconnect_requested\":true}");
}
std::atomic<bool> wifiTesting{false};
esp_err_t testWifi(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  if (wifiTesting.exchange(true))
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "Wi-Fi test already active");
  if (xTaskCreate(
          [](void *) {
            vTaskDelay(pdMS_TO_TICKS(400));
            esp_wifi_stop();
            vTaskDelay(pdMS_TO_TICKS(8000));
            esp_wifi_start();
            WiFi.reconnect();
            wifiTesting = false;
            vTaskDelete(nullptr);
          },
          "v51_wifi_test", 4096, nullptr, 1, nullptr) != pdPASS) {
    wifiTesting = false;
    return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "Test task unavailable");
  }
  return jsonResponse(r, "{\"wifi_outage_ms\":8000}");
}
esp_err_t getSystem(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  JsonDocument doc;
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  doc["gpio47"] = gpio_get_level(GPIO_NUM_47);
  doc["rtc_utc_available"] = clockStamp().utc != 0;
  return jsonResponse(r, encode(doc));
}
esp_err_t reboot(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  auto result = jsonResponse(r, "{\"restart\":true}");
  xTaskCreate(
      [](void *) {
        vTaskDelay(pdMS_TO_TICKS(300));
        ESP.restart();
      },
      "v51_restart", 3072, nullptr, 1, nullptr);
  return result;
}
} // namespace
void recordDecision(const EnergyState &e, uint32_t now,
                    const std::string &record) {
  const std::lock_guard guard(stateLock);
  snapshot = e;
  sampleAt = now;
  latest = record;
  if (history && record.size() < recordSize) {
    auto *p = history + (cursor++ % records) * recordSize;
    memcpy(p, record.c_str(), record.size() + 1);
  }
}
bool mqttConnected() { return connected.load(); }
uint32_t mqttReconnectCount() { return reconnectCount.load(); }

void registerM4(httpd_handle_t server, Authorize auth, Preferences &storage) {
  nvs = &storage;
  authorized = auth;
  Broker candidate;
  if (storage.getBytesLength("mqtt-v1") == sizeof(candidate) &&
      storage.getBytes("mqtt-v1", &candidate, sizeof(candidate)) ==
          sizeof(candidate) &&
      validBroker(candidate))
    configured = candidate;
  else if (storage.getBytesLength("mqtt-prev") == sizeof(candidate) &&
           storage.getBytes("mqtt-prev", &candidate, sizeof(candidate)) ==
               sizeof(candidate) &&
           validBroker(candidate))
    configured = candidate;
  history = static_cast<char *>(ps_calloc(records, recordSize));
  const httpd_uri_t routes[] = {
      {"/api/v1/telemetry", HTTP_GET, getTelemetry, nullptr},
      {"/api/v1/ems/decision", HTTP_GET, getDecision, nullptr},
      {"/api/v1/ems/decisions", HTTP_GET, getDecisions, nullptr},
      {"/api/v1/mqtt", HTTP_GET, getBroker, nullptr},
      {"/api/v1/mqtt", HTTP_POST, saveBroker, nullptr},
      {"/api/v1/system", HTTP_GET, getSystem, nullptr},
      {"/api/v1/reboot", HTTP_POST, reboot, nullptr},
      {"/api/v1/mqtt/test", HTTP_POST, testBroker, nullptr},
      {"/api/v1/system/wifi-test", HTTP_POST, testWifi, nullptr},
  };
  for (auto &route : routes)
    if (httpd_register_uri_handler(server, &route) != ESP_OK)
      recordLog("M4 route registration failed");
  xTaskCreate(brokerTask, "v51_mqtt", 10240, nullptr, 1, nullptr);
}
} // namespace heliograph::v51
