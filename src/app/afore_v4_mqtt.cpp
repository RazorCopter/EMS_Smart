// SPDX-License-Identifier: MIT

#include "afore_v4_mqtt.h"

#include <ArduinoJson.h>
#include <cstdio>

#include "device/measurement.h"
#include "device/telemetry_quality.h"
#include "json_limits.h"

namespace heliograph {
namespace {

/// Converts canonical measurement ID to Home Assistant entity ID fragment
/// "battery.soc" -> "battery_soc"
std::string toEntityId(const std::string& measurementId) {
    std::string entityId = measurementId;
    for (char& c : entityId) {
        if (c == '.') c = '_';
    }
    return entityId;
}

/// Maps MeasurementType to HA device_class
const char* deviceClassForType(MeasurementType type) {
    switch (type) {
        case MeasurementType::Power:          return "power";
        case MeasurementType::Voltage:        return "voltage";
        case MeasurementType::Current:        return "current";
        case MeasurementType::Frequency:      return "frequency";
        case MeasurementType::Temperature:    return "temperature";
        case MeasurementType::Energy:         return "energy";
        case MeasurementType::Ratio:          return "battery";  // SoC
        default:                              return nullptr;
    }
}

}  // namespace

AforeV4MqttPublisher::AforeV4MqttPublisher(const Config& config)
    : config_(config) {
    stateTopic_ = config_.baseTopic + "/state";
    shadowStateTopic_ = config_.baseTopic + "/shadow";
    availabilityTopic_ = config_.baseTopic + "/availability";
}

std::vector<mqtt::DiscoveryEntity> AforeV4MqttPublisher::buildDiscoveryMessages() {
    std::vector<mqtt::DiscoveryEntity> entities;
    
    // Battery measurements (VERIFIED)
    entities.push_back(buildSensorEntity(
        measurement_id::kBatterySoc, "Battery SOC", "battery", "measurement", "%", true));
    entities.push_back(buildSensorEntity(
        measurement_id::kBatteryPower, "Battery Power", "power", "measurement", "W", true));
    entities.push_back(buildSensorEntity(
        measurement_id::kBatteryVoltage, "Battery Voltage", "voltage", "measurement", "V", true));
    entities.push_back(buildSensorEntity(
        measurement_id::kBatteryCurrent, "Battery Current", "current", "measurement", "A", true));
    
    // PV measurements (VERIFIED powers)
    entities.push_back(buildSensorEntity(
        measurement_id::kDcPowerTotal, "Total PV Power", "power", "measurement", "W", true));
    entities.push_back(buildSensorEntity(
        measurement_id::kDcMppt1Power, "PV1 Power", "power", "measurement", "W", true));
    entities.push_back(buildSensorEntity(
        measurement_id::kDcMppt2Power, "PV2 Power", "power", "measurement", "W", true));
    
    // Grid and inverter (VERIFIED)
    entities.push_back(buildSensorEntity(
        measurement_id::kGridPower, "Grid Power", "power", "measurement", "W", true));
    entities.push_back(buildSensorEntity(
        measurement_id::kAcPowerTotal, "Inverter Output", "power", "measurement", "W", true));
    
    // UNVERIFIED measurements (optional, diagnostic)
    if (config_.enableUnverified) {
        entities.push_back(buildSensorEntity(
            measurement_id::kDcMppt1Voltage, "PV1 Voltage", "voltage", "measurement", "V", false));
        entities.push_back(buildSensorEntity(
            measurement_id::kDcMppt1Current, "PV1 Current", "current", "measurement", "A", false));
        entities.push_back(buildSensorEntity(
            measurement_id::kDcMppt2Voltage, "PV2 Voltage", "voltage", "measurement", "V", false));
        entities.push_back(buildSensorEntity(
            measurement_id::kDcMppt2Current, "PV2 Current", "current", "measurement", "A", false));
        entities.push_back(buildSensorEntity(
            measurement_id::kAcL1Voltage, "Grid Voltage", "voltage", "measurement", "V", false));
        entities.push_back(buildSensorEntity(
            measurement_id::kAcFrequency, "Grid Frequency", "frequency", "measurement", "Hz", false));
        entities.push_back(buildSensorEntity(
            measurement_id::kTemperature, "Inverter Temperature", "temperature", "measurement", "°C", false));
    }
    
    // Shadow controller entities
    entities.push_back(buildShadowSensorEntity(
        "shadow_mode", "Shadow Mode", "{{ value_json.mode }}"));
    entities.push_back(buildShadowSensorEntity(
        "shadow_quality", "Overall Quality", "{{ value_json.quality_summary }}"));
    entities.push_back(buildShadowSensorEntity(
        "shadow_success_rate", "Poll Success Rate", "{{ value_json.success_rate }}"));
    
    return entities;
}

mqtt::DiscoveryEntity AforeV4MqttPublisher::buildSensorEntity(
        const std::string& measurementId,
        const std::string& name,
        const std::string& deviceClass,
        const std::string& stateClass,
        const std::string& unit,
        bool verified) {
    
    std::string entityId = toEntityId(measurementId);
    std::string uniqueId = config_.nodeId + "_" + entityId;
    
    // Build config topic: homeassistant/sensor/afore_v4_battery_soc/config
    std::string configTopic = config_.discoveryPrefix + "/sensor/" + uniqueId + "/config";
    
    // Build JSON payload
    StaticJsonDocument<512> doc;
    doc["name"] = name;
    doc["unique_id"] = uniqueId;
    doc["state_topic"] = stateTopic_;
    doc["value_template"] = "{{ value_json." + entityId + " }}";
    doc["availability_topic"] = availabilityTopic_;
    doc["qos"] = 0;
    
    if (deviceClass != nullptr && deviceClass[0] != '\0') {
        doc["device_class"] = deviceClass;
    }
    if (stateClass != nullptr && stateClass[0] != '\0') {
        doc["state_class"] = stateClass;
    }
    if (!unit.empty()) {
        doc["unit_of_measurement"] = unit;
    }
    
    // Device info
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"].add(config_.nodeId);
    device["name"] = "Afore AF5K-SL (Shadow)";
    device["manufacturer"] = "Afore";
    device["model"] = "AF5K-SL (Shadow Mode)";
    device["sw_version"] = "Phase 0 CP3";
    
    // Mark UNVERIFIED entities
    if (!verified) {
        doc["entity_category"] = "diagnostic";
        doc["enabled_by_default"] = false;
    }
    
    // JSON attributes for quality metadata
    doc["json_attributes_topic"] = stateTopic_;
    doc["json_attributes_template"] = 
        "{{ {'quality': value_json._quality." + entityId + ", "
        "'age_ms': value_json._age_ms." + entityId + ", "
        "'verified': " + (verified ? "true" : "false") + "} | tojson }}";
    
    std::string payload;
    serializeJson(doc, payload);
    
    return mqtt::DiscoveryEntity{configTopic, uniqueId, payload};
}

mqtt::DiscoveryEntity AforeV4MqttPublisher::buildShadowSensorEntity(
        const std::string& entityId,
        const std::string& name,
        const std::string& valueTemplate) {
    
    std::string uniqueId = config_.nodeId + "_" + entityId;
    std::string configTopic = config_.discoveryPrefix + "/sensor/" + uniqueId + "/config";
    
    StaticJsonDocument<384> doc;
    doc["name"] = name;
    doc["unique_id"] = uniqueId;
    doc["state_topic"] = shadowStateTopic_;
    doc["value_template"] = valueTemplate;
    doc["availability_topic"] = availabilityTopic_;
    doc["entity_category"] = "diagnostic";
    doc["qos"] = 0;
    
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"].add(config_.nodeId);
    device["name"] = "Afore AF5K-SL (Shadow)";
    device["manufacturer"] = "Afore";
    device["model"] = "AF5K-SL (Shadow Mode)";
    
    std::string payload;
    serializeJson(doc, payload);
    
    return mqtt::DiscoveryEntity{configTopic, uniqueId, payload};
}

std::string AforeV4MqttPublisher::buildTelemetryPayload(const AforeTelemetryEngine& engine) {
    const auto& samples = engine.getTelemetrySamples();
    
    DynamicJsonDocument doc(2048);
    JsonObject quality = doc.createNestedObject("_quality");
    JsonObject ageMs = doc.createNestedObject("_age_ms");
    
    for (const auto& [id, sample] : samples) {
        std::string entityId = toEntityId(id);
        
        // Publish value as null if not publishable (Invalid)
        if (sample.publishable()) {
            doc[entityId] = sample.value;
        } else {
            doc[entityId] = nullptr;
        }
        
        // Quality and age metadata
        quality[entityId] = qualityToString(sample.quality);
        ageMs[entityId] = sample.ageMs;
    }
    
    std::string payload;
    serializeJson(doc, payload);
    return payload;
}

std::string AforeV4MqttPublisher::buildShadowStatePayload(
        const AforeTelemetryEngine& engine, uint64_t nowMs) {
    
    const auto& samples = engine.getTelemetrySamples();
    const auto& stats = engine.getStats();
    
    // Compute quality summary
    size_t actionable = 0, stale = 0, invalid = 0;
    Quality worstQuality = Quality::Good;
    
    for (const auto& [id, sample] : samples) {
        if (sample.actionable()) {
            actionable++;
        } else if (sample.quality == Quality::Stale) {
            stale++;
        } else if (sample.quality == Quality::Invalid) {
            invalid++;
        }
        
        if (sample.quality > worstQuality) {
            worstQuality = sample.quality;
        }
    }
    
    StaticJsonDocument<512> doc;
    doc["mode"] = "shadow";
    doc["quality_summary"] = qualityToString(worstQuality);
    doc["actionable_measurements"] = actionable;
    doc["stale_measurements"] = stale;
    doc["invalid_measurements"] = invalid;
    doc["success_rate"] = stats.successRate();
    doc["last_poll_ms"] = stats.lastSuccessMs;
    doc["timestamp_ms"] = nowMs;
    
    std::string payload;
    serializeJson(doc, payload);
    return payload;
}

std::string AforeV4MqttPublisher::buildAvailabilityPayload(bool online) const {
    return online ? "online" : "offline";
}

}  // namespace heliograph
