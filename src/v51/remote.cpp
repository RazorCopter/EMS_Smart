// SPDX-License-Identifier: MIT
#include "remote.h"
#include "clock_service.h"
#include <sstream>
#include "ems_runtime.h"
#include "m4.h"
#include "m5.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <sdkconfig.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

#include "ota_public_key.h"

#ifndef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error "The network prototype requires bootloader rollback enabled."
#endif

#ifndef V51_BUILD_ID
#define V51_BUILD_ID "v5.1-remote-1"
#endif

extern "C" bool verifyRollbackLater() { return true; }

namespace heliograph::v51 {
namespace {
constexpr char kHost[] = "afore-ems";
constexpr char kApName[] = "Afore-EMS-Setup";
constexpr char kNvsNamespace[] = "afore-v51";
constexpr uint32_t kBootConfirmTimeout = 90000;
constexpr uint32_t kBootStableTime = 10000;
constexpr size_t kAdminMinLength = 12;
constexpr size_t kAdminMaxLength = 64;
constexpr char kAdminKey[] = "admin-pass";
constexpr size_t kMaxSlot = 0x600000;

struct StationSettings {
    char ssid[33]{};
    char password[65]{};
};

struct LogEntry {
    uint32_t sequence = 0;
    ClockStamp clock{};
    char message[768]{};
};

Preferences preferences;
StationSettings station;
char setupPassword[25]{};
char adminPassword[kAdminMaxLength + 1]{};
std::mutex adminMutex;
httpd_handle_t server = nullptr;
std::atomic<bool> sawHealth{false};
std::atomic<bool> restartRequested{false};
std::atomic<esp_ota_img_states_t> bootState{ESP_OTA_IMG_UNDEFINED};
bool mdns = false;
bool accessPoint = false;
bool online = false;
uint32_t lastConnection = 0;
uint32_t reconnectAt = 0;
uint32_t bootStarted = 0;

std::mutex logMutex;
std::array<LogEntry, 64> logs;
uint32_t sequence = 0;

int nibble(char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
    return -1;
}

bool fromHex(const char* text, uint8_t* output, size_t count) {
    if (strlen(text) != 2 * count) return false;
    for (size_t i = 0; i < count; ++i) {
        const int high = nibble(text[2 * i]);
        const int low = nibble(text[2 * i + 1]);
        if (high < 0 || low < 0) return false;
        output[i] = static_cast<uint8_t>(high * 16 + low);
    }
    return true;
}

bool equalBytes(const uint8_t* left, const uint8_t* right, size_t size) {
    uint8_t difference = 0;
    for (size_t i = 0; i < size; ++i) {
        difference |= left[i] ^ right[i];
    }
    return difference == 0;
}

bool adminMatches(const char* candidate, size_t length) {
    const std::lock_guard lock(adminMutex);
    return adminPassword[0] && length == strlen(adminPassword) &&
        equalBytes(reinterpret_cast<const uint8_t*>(candidate),
                   reinterpret_cast<const uint8_t*>(adminPassword), length);
}

bool authorized(httpd_req_t* request) {
    constexpr size_t prefixLength = 6;
    constexpr size_t decodedCapacity = prefixLength + kAdminMaxLength;
    char header[prefixLength + 4 * ((decodedCapacity + 2) / 3) + 1]{};
    uint8_t decoded[decodedCapacity]{};
    size_t length = 0;
    if (httpd_req_get_hdr_value_str(request, "Authorization", header, sizeof(header)) == ESP_OK &&
        strncmp(header, "Basic ", prefixLength) == 0 &&
        mbedtls_base64_decode(decoded, sizeof(decoded), &length,
                              reinterpret_cast<const uint8_t*>(header + prefixLength),
                              strlen(header + prefixLength)) == 0 &&
        length >= prefixLength && memcmp(decoded, "admin:", prefixLength) == 0 &&
        adminMatches(reinterpret_cast<const char*>(decoded) + prefixLength, length - prefixLength)) {
        return true;
    }
    httpd_resp_set_status(request, "401 Unauthorized");
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Basic realm=\"Afore EMS\"");
    httpd_resp_sendstr(request, "Authentication required");
    return false;
}

bool validAdminPassword(const char* value, size_t length) {
    if (length < kAdminMinLength || length > kAdminMaxLength) return false;
    bool nonSpace = false;
    for (size_t i = 0; i < length; ++i) {
        const auto ch = static_cast<unsigned char>(value[i]);
        if (ch < 0x20 || ch > 0x7E) return false;
        nonSpace |= ch != ' ';
    }
    return nonSpace;
}

const char* imageState(esp_ota_img_states_t state) {
    switch (state) {
        case ESP_OTA_IMG_NEW: return "new";
        case ESP_OTA_IMG_PENDING_VERIFY: return "pending_verify";
        case ESP_OTA_IMG_VALID: return "valid";
        case ESP_OTA_IMG_INVALID: return "invalid";
        case ESP_OTA_IMG_ABORTED: return "aborted";
        default: return "undefined";
    }
}

esp_err_t health(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    const bool sta = WiFi.status() == WL_CONNECTED;
    const auto* running = esp_ota_get_running_partition();
    const auto staIp = sta ? WiFi.localIP().toString() : String();
    const auto res = sampleSystemResources();
    char rssi[12]{};
    if (sta) snprintf(rssi, sizeof(rssi), "%d", WiFi.RSSI());
    else strcpy(rssi, "null");
    char cpuPct[12]{}, cpu0Pct[12]{}, cpu1Pct[12]{};
    if (res.cpuValid) {
        snprintf(cpuPct, sizeof(cpuPct), "%.1f", res.cpuPct);
        snprintf(cpu0Pct, sizeof(cpu0Pct), "%.1f", res.cpu0Pct);
        snprintf(cpu1Pct, sizeof(cpu1Pct), "%.1f", res.cpu1Pct);
    } else {
        strcpy(cpuPct, "null");
        strcpy(cpu0Pct, "null");
        strcpy(cpu1Pct, "null");
    }

    char response[1280]{};
    snprintf(response, sizeof(response),
             "{\"firmware\":\"%s\",\"mode\":\"SHADOW\",\"uptime_ms\":%lu,\"reset_reason\":%d,"
             "\"heap\":%u,\"heap_total\":%u,\"min_heap\":%u,\"heap_max_alloc\":%u,\"heap_used_pct\":%.1f,"
             "\"psram\":%u,\"psram_free\":%u,\"psram_min\":%u,\"psram_used_pct\":%.1f,"
             "\"cpu_pct\":%s,\"cpu0_pct\":%s,\"cpu1_pct\":%s,\"cpu_mhz\":%u,"
             "\"task_count\":%u,\"min_task_stack_free_b\":%u,"
             "\"nvs_used_entries\":%u,\"nvs_free_entries\":%u,\"nvs_total_entries\":%u,\"nvs_used_pct\":%.1f,"
             "\"flash_size\":%u,\"sketch_size\":%u,\"sketch_free\":%u,"
             "\"sta_connected\":%s,\"ip\":\"%s\","
             "\"rssi_dbm\":%s,\"ota_slot\":\"%s\",\"ota_state\":\"%s\","
             "\"aux1\":\"%s\",\"executed\":false,\"modbus_write_count\":0}",
             V51_BUILD_ID, static_cast<unsigned long>(millis()), int(esp_reset_reason()),
             res.heapFree, res.heapTotal, res.heapMin, res.heapMaxAlloc, res.heapUsedPct,
             res.psramTotal, res.psramFree, res.psramMin, res.psramUsedPct,
             cpuPct, cpu0Pct, cpu1Pct, res.cpuMhz,
             res.taskCount, res.minTaskStackFreeB,
             res.nvsUsedEntries, res.nvsFreeEntries, res.nvsTotalEntries, res.nvsUsedPct,
             res.flashSize, res.sketchSize, res.sketchFree,
             sta ? "true" : "false",
             staIp.c_str(), rssi, running ? running->label : "unknown", imageState(bootState.load()), auxOn()?"ON":"OFF");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const auto result = httpd_resp_sendstr(request, response);
    if (sta && result == ESP_OK) sawHealth.store(true);
    return result;
}

esp_err_t logResponse(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    uint32_t end;
    {
        const std::lock_guard lock(logMutex);
        end = sequence;
    }
    const uint32_t begin = end >= logs.size() ? end - logs.size() + 1 : 1;
    httpd_resp_set_type(request, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    for (uint32_t id = begin; id <= end; ++id) {
        LogEntry entry;
        {
            const std::lock_guard lock(logMutex);
            entry = logs[(id - 1) % logs.size()];
        }
        if (entry.sequence != id) continue;
        char row[940];
        std::ostringstream timing;
        appendClock(timing,entry.clock);
        snprintf(row, sizeof(row), "%lu %lu %s {%s}\n", static_cast<unsigned long>(id),
                 static_cast<unsigned long>(entry.clock.uptime), entry.message, timing.str().c_str()+1);
        if (httpd_resp_sendstr_chunk(request, row) != ESP_OK) return ESP_FAIL;
    }
    return httpd_resp_send_chunk(request, nullptr, 0);
}

constexpr char kSetupPage[] = R"HTML(
<!doctype html><html lang="it"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Afore EMS · Setup</title><style>
body{background:#101815;color:#eef5f0;font:16px system-ui;margin:0;padding:24px}
main{max-width:520px;margin:5vh auto}form{background:#192522;padding:22px;border:1px solid #3c554c;border-radius:12px}
label,input{display:block}label{margin:14px 0 6px}input{box-sizing:border-box;width:100%;padding:10px;font:inherit}
button{margin-top:16px;padding:10px 14px;background:#9de3bf;border:0;border-radius:5px;font:inherit;color:#102019}
a{color:#b1e8cc;margin-right:16px}
</style><main><h1>Afore EMS · Setup</h1><p>SHADOW · AUX1 disabilitata</p>
<form action="/setup" method="post"><label for="ssid">Rete Wi-Fi (2,4 GHz)</label>
<input id="ssid" name="ssid" maxlength="32" required autocomplete="off">
<label for="key">Password Wi-Fi</label><input id="key" name="key" type="password" maxlength="64" autocomplete="new-password">
<button>Salva e riavvia</button></form><p><a href="/#system">Cambia password admin</a><a href="/api/v1/health">Stato ESP32</a><a href="/api/v1/logs">Log</a></p></main></html>
)HTML";

esp_err_t setupPage(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    if (strcmp(request->uri,"/wifi")==0) return httpd_resp_sendstr(request,kSetupPage);
    httpd_resp_set_hdr(request,"Content-Encoding","gzip");
    return httpd_resp_send(request,reinterpret_cast<const char*>(dashboardPage()),dashboardSize());
}

bool receiveBody(httpd_req_t* request, char* buffer, size_t capacity) {
    if (!request->content_len || request->content_len >= capacity) return false;
    size_t received = 0;
    while (received < request->content_len) {
        const int count = httpd_req_recv(request, buffer + received, request->content_len - received);
        if (count <= 0) return false;
        received += count;
    }
    buffer[received] = '\0';
    return true;
}

esp_err_t changeAdminPassword(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;

    char type[32]{};
    char body[1200]{};
    JsonDocument data;
    if (httpd_req_get_hdr_value_str(request, "Content-Type", type, sizeof(type)) != ESP_OK ||
        strcmp(type, "application/json") != 0 ||
        !receiveBody(request, body, sizeof(body)) ||
        deserializeJson(data, body, request->content_len) ||
        !data["current_password"].is<const char*>() || !data["new_password"].is<const char*>()) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Richiesta di cambio password non valida.");
    }
    const auto current = data["current_password"].as<JsonString>();
    const auto next = data["new_password"].as<JsonString>();
    if (!adminMatches(current.c_str(), current.size())) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "La password attuale non è corretta.");
    }
    if (!validAdminPassword(next.c_str(), next.size())) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Usa da 12 a 64 caratteri ASCII stampabili.");
    }
    if (adminMatches(next.c_str(), next.size())) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Scegli una password diversa da quella attuale.");
    }
    {
        const std::lock_guard lock(adminMutex);
        if (preferences.putString(kAdminKey, next.c_str()) != next.size()) {
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Salvataggio password non riuscito.");
        }
        memcpy(adminPassword, next.c_str(), next.size() + 1);
    }
    recordLog("Admin password changed");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, "{\"updated\":true}");
}

bool decodeFormValue(const char* input, char* output, size_t size) {
    size_t length = 0;
    while (*input) {
        char value = *input++;
        if (value == '+') value = ' ';
        else if (value == '%') {
            if (!input[0] || !input[1]) return false;
            int high = nibble(input[0]), low = nibble(input[1]);
            if (high < 0 || low < 0) return false;
            value = static_cast<char>((high << 4) + low);
            input += 2;
        }
        if (!value || length + 1 >= size) return false;
        output[length++] = value;
    }
    output[length] = '\0';
    return true;
}

esp_err_t saveStation(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    char form[360]{};
    char type[80]{};
    StationSettings settings{};
    if (httpd_req_get_hdr_value_str(request, "Content-Type", type, sizeof(type)) != ESP_OK ||
        strcmp(type, "application/x-www-form-urlencoded") != 0 ||
        !receiveBody(request, form, sizeof(form)) || strncmp(form, "ssid=", 5) != 0) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid setup form");
    }
    char* separator = strchr(form, '&');
    if (separator == nullptr || strncmp(separator, "&key=", 5) != 0) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Wi-Fi key field missing");
    }
    *separator = '\0';
    if (!decodeFormValue(form + 5, settings.ssid, sizeof(settings.ssid)) || !settings.ssid[0] ||
        !decodeFormValue(separator + 5, settings.password, sizeof(settings.password))) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi credentials");
    }
    const auto keySize = strlen(settings.password);
    if (keySize && keySize < 8) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Wi-Fi password too short");
    }
    if (preferences.putBytes("station", &settings, sizeof(settings)) != sizeof(settings)) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }
    recordLog("Station credentials saved; reboot requested");
    const auto result = httpd_resp_sendstr(request, "Wi-Fi salvato. Dopo il riavvio apri http://afore-ems.local/");
    restartRequested.store(true);
    return result;
}

bool verifiedManifest(httpd_req_t* request, std::array<uint8_t, 32>& digest) {
    char sha[65]{};
    char signature[145]{};
    std::array<uint8_t, 72> der{};
    if (httpd_req_get_hdr_value_str(request, "X-Firmware-SHA256", sha, sizeof(sha)) != ESP_OK ||
        !fromHex(sha, digest.data(), digest.size()) ||
        httpd_req_get_hdr_value_str(request, "X-Firmware-Signature", signature, sizeof(signature)) != ESP_OK) {
        return false;
    }
    const size_t size = strlen(signature);
    if (size == 0 || size % 2 || !fromHex(signature, der.data(), size / 2)) return false;
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    const bool valid = mbedtls_pk_parse_public_key(
        &key, reinterpret_cast<const uint8_t*>(kOtaPublicKey), sizeof(kOtaPublicKey)) == 0 &&
        mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest.data(), digest.size(), der.data(), size / 2) == 0;
    mbedtls_pk_free(&key);
    return valid;
}

esp_err_t updateFirmware(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    if (bootState.load() == ESP_OTA_IMG_PENDING_VERIFY || restartRequested.load()) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "Boot confirmation or restart pending");
    }
    std::array<uint8_t, 32> expected{};
    if (request->content_len == 0 || request->content_len > kMaxSlot || !verifiedManifest(request, expected)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid image size, digest or signature");
    }
    const auto* target = esp_ota_get_next_update_partition(nullptr);
    esp_ota_handle_t handle{};
    if (!target || esp_ota_begin(target, request->content_len, &handle) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA partition unavailable");
    }

    std::array<uint8_t, 1536> buffer{};
    std::array<uint8_t, 32> actual{};
    mbedtls_sha256_context hash;
    mbedtls_sha256_init(&hash);
    bool ok = mbedtls_sha256_starts(&hash, 0) == 0;
    size_t received = 0;
    const uint32_t start = millis();
    while (ok && received < request->content_len && millis() - start < 120000) {
        const int length = httpd_req_recv(request, reinterpret_cast<char*>(buffer.data()),
                                        std::min(buffer.size(), request->content_len - received));
        if (length <= 0 || (received == 0 && buffer[0] != 0xe9)) break;
        ok = mbedtls_sha256_update(&hash, buffer.data(), length) == 0 &&
             esp_ota_write(handle, buffer.data(), length) == ESP_OK;
        received += length;
    }
    ok = ok && received == request->content_len && mbedtls_sha256_finish(&hash, actual.data()) == 0 &&
         equalBytes(actual.data(), expected.data(), actual.size());
    mbedtls_sha256_free(&hash);
    if (!ok) {
        esp_ota_abort(handle);
        recordLog("OTA rejected: transfer incomplete or hash mismatch");
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "OTA transfer or hash failed");
    }
    if (esp_ota_end(handle) != ESP_OK || esp_ota_set_boot_partition(target) != ESP_OK) {
        recordLog("OTA rejected: image verification or slot selection failed");
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA image rejected");
    }
    recordLog("Signed OTA image staged; reboot requested");
    httpd_resp_set_type(request, "application/json");
    const auto result = httpd_resp_sendstr(request, "{\"status\":\"staged\",\"confirmation_required\":true}");
    restartRequested.store(true);
    return result;
}

bool startHttp() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 10240;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.max_uri_handlers = 28;
    config.lru_purge_enable = true;
    if (httpd_start(&server, &config) != ESP_OK) return false;
    const httpd_uri_t routes[] = {
        {"/", HTTP_GET, setupPage, nullptr},
        {"/wifi", HTTP_GET, setupPage, nullptr},
        {"/setup", HTTP_POST, saveStation, nullptr},
        {"/api/v1/health", HTTP_GET, health, nullptr},
        {"/api/v1/logs", HTTP_GET, logResponse, nullptr},
        {"/api/v1/ota", HTTP_POST, updateFirmware, nullptr},
        {"/api/v1/system/admin-password", HTTP_POST, changeAdminPassword, nullptr},
    };
    for (const auto& route : routes) {
        if (httpd_register_uri_handler(server, &route) != ESP_OK) {
            httpd_stop(server);
            server = nullptr;
            return false;
        }
    }
    registerEms(server,authorized);
    registerM4(server,authorized,preferences);
    registerM5(server,authorized);
    return registerClock(server,authorized);
}

void startFallback() {
    if (accessPoint) return;
    WiFi.mode(WIFI_AP_STA);
    accessPoint = WiFi.softAP(kApName, setupPassword);
    recordLog(accessPoint ? "Setup AP available at 192.168.4.1" : "Setup AP startup failed");
}

void beginStation() {
    reconnectAt = millis();
    if (station.ssid[0]) WiFi.begin(station.ssid, station.password);
}

void manageWifi(uint32_t now) {
    if (WiFi.status() == WL_CONNECTED) {
        if (!online) {
            online = true;
            const auto address = WiFi.localIP().toString();
            char line[96];
            snprintf(line, sizeof(line), "Wi-Fi STA ip=%s rssi=%d", address.c_str(), WiFi.RSSI());
            recordLog(line);
        }
        lastConnection = now;
        if (!mdns && (mdns = MDNS.begin(kHost))) MDNS.addService("http", "tcp", 80);
        if (accessPoint) {
            WiFi.softAPdisconnect(true);
            accessPoint = false;
        }
        return;
    }
    if (online) {
        online = false;
        if (mdns) MDNS.end();
        mdns = false;
        recordLog("Wi-Fi STA disconnected");
    }
    if (!station.ssid[0] || now - lastConnection >= 15000) startFallback();
    if (station.ssid[0] && now - reconnectAt >= 12000) beginStation();
}

void verifyBoot(uint32_t now, bool healthy) {
    if (bootState.load() != ESP_OTA_IMG_PENDING_VERIFY) return;
#ifdef V51_TEST_UNCONFIRMED
    if (now - bootStarted > 20000) ESP.restart();
#else
    if (now - bootStarted >= kBootStableTime && healthy && server && online && sawHealth.load()) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            bootState.store(ESP_OTA_IMG_VALID);
            recordLog("OTA boot confirmed: watchdog, Wi-Fi STA and authenticated health check OK");
            return;
        }
    }
#endif
    if (now - bootStarted > kBootConfirmTimeout) {
        recordLog("OTA boot confirmation timed out; rollback");
        esp_ota_mark_app_invalid_rollback_and_reboot();
        ESP.restart();
    }
}
}

bool beginRemote() {
    bootStarted = millis();
    lastConnection = bootStarted;
    esp_ota_img_states_t state;
    const auto* running = esp_ota_get_running_partition();
    if (running && esp_ota_get_state_partition(running, &state) == ESP_OK) bootState.store(state);
    if (!preferences.begin(kNvsNamespace, false)) {
        recordLog("NVS initialization failed");
        return false;
    }

    loadPolicy(preferences);
    loadClock(preferences);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    if (preferences.getString("manage", setupPassword, sizeof(setupPassword)) != sizeof(setupPassword) ||
        strnlen(setupPassword, sizeof(setupPassword)) != sizeof(setupPassword) - 1) {
        constexpr char digits[] = "0123456789abcdef";
        uint8_t random[12];
        esp_fill_random(random, sizeof(random));
        for (size_t i = 0; i < sizeof(random); ++i) {
            setupPassword[i * 2] = digits[random[i] >> 4];
            setupPassword[i * 2 + 1] = digits[random[i] & 15];
        }
        setupPassword[24] = '\0';
        if (preferences.putString("manage", setupPassword) != sizeof(setupPassword) - 1) {
            recordLog("Management password persistence failed");
            return false;
        }
    }
    const auto storedAdminLength = preferences.getString(kAdminKey, adminPassword, sizeof(adminPassword));
    if (!storedAdminLength || !validAdminPassword(adminPassword, strlen(adminPassword))) {
        memcpy(adminPassword, setupPassword, sizeof(setupPassword));
    }
    if (preferences.getBytesLength("station") == sizeof(station)) {
        preferences.getBytes("station", &station, sizeof(station));
        station.ssid[sizeof(station.ssid) - 1] = '\0';
        station.password[sizeof(station.password) - 1] = '\0';
    }
    WiFi.setHostname(kHost);
    WiFi.setAutoReconnect(true);
    beginStation();
    if (!station.ssid[0]) startFallback();
    return startHttp();
}

void tickRemote(bool healthy) {
    if (restartRequested.load()) {
        delay(250);
        ESP.restart();
    }
    static uint32_t lastResourceSample = 0;
    const uint32_t now = millis();
    if (now - lastResourceSample >= 5000) {
        lastResourceSample = now;
        sampleSystemResources();
    }
    if (server) manageWifi(now);
    if (server) serviceClock(online);
    verifyBoot(now, healthy);
}

SystemResources sampleSystemResources() {
    static std::mutex resMutex;
    static bool hasBaseline = false;
    static uint32_t lastSampleMs = 0;
    static uint32_t prevTotal = 0;
    static uint32_t prevIdle0 = 0;
    static uint32_t prevIdle1 = 0;
    static SystemResources cached{};

    const std::lock_guard lock(resMutex);
    const uint32_t nowMs = millis();
    if (hasBaseline && (nowMs - lastSampleMs) < 2000 && cached.heapTotal > 0) {
        return cached;
    }

    SystemResources out = cached;
    out.cpuMhz = ESP.getCpuFreqMHz();
    out.heapTotal = ESP.getHeapSize();
    out.heapFree = ESP.getFreeHeap();
    out.heapMin = ESP.getMinFreeHeap();
    out.heapMaxAlloc = ESP.getMaxAllocHeap();
    out.heapUsedPct = out.heapTotal ? std::roundf(1000.0f * float(out.heapTotal - out.heapFree) / float(out.heapTotal)) / 10.0f : 0.0f;
    out.psramTotal = ESP.getPsramSize();
    out.psramFree = ESP.getFreePsram();
    out.psramMin = ESP.getMinFreePsram();
    out.psramUsedPct = out.psramTotal ? std::roundf(1000.0f * float(out.psramTotal - out.psramFree) / float(out.psramTotal)) / 10.0f : 0.0f;
    out.flashSize = ESP.getFlashChipSize();
    out.sketchSize = ESP.getSketchSize();
    out.sketchFree = ESP.getFreeSketchSpace();

    nvs_stats_t nvsStats{};
    if (nvs_get_stats(nullptr, &nvsStats) == ESP_OK) {
        out.nvsUsedEntries = static_cast<uint32_t>(nvsStats.used_entries);
        out.nvsFreeEntries = static_cast<uint32_t>(nvsStats.free_entries);
        out.nvsTotalEntries = static_cast<uint32_t>(nvsStats.total_entries);
        out.nvsUsedPct = out.nvsTotalEntries
            ? std::roundf(1000.0f * float(out.nvsUsedEntries) / float(out.nvsTotalEntries)) / 10.0f
            : 0.0f;
    }

    TaskStatus_t tasks[24]{};
    uint32_t totalRunTime = 0;
    const UBaseType_t count = uxTaskGetSystemState(tasks, sizeof(tasks) / sizeof(tasks[0]), &totalRunTime);
    out.taskCount = static_cast<uint32_t>(count);
    uint32_t minStack = UINT32_MAX;
    uint32_t idle0 = 0;
    uint32_t idle1 = 0;
    bool found0 = false;
    bool found1 = false;
    for (UBaseType_t i = 0; i < count; ++i) {
        const uint32_t freeStack = static_cast<uint32_t>(tasks[i].usStackHighWaterMark) * sizeof(StackType_t);
        if (freeStack < minStack) minStack = freeStack;
        if (strcmp(tasks[i].pcTaskName, "IDLE0") == 0) {
            idle0 = tasks[i].ulRunTimeCounter;
            found0 = true;
        } else if (strcmp(tasks[i].pcTaskName, "IDLE1") == 0) {
            idle1 = tasks[i].ulRunTimeCounter;
            found1 = true;
        }
    }
    out.minTaskStackFreeB = (minStack == UINT32_MAX) ? 0 : minStack;

    if (hasBaseline && (nowMs - lastSampleMs) >= 500) {
        const uint32_t deltaTotal = totalRunTime - prevTotal;
        if (deltaTotal > 0) {
            auto coreLoad = [deltaTotal](uint32_t cur, uint32_t prev) -> float {
                const uint32_t deltaIdle = cur - prev;
                const float idleFrac = std::min(1.0f, std::max(0.0f, float(deltaIdle) / float(deltaTotal)));
                return std::roundf(1000.0f * (1.0f - idleFrac)) / 10.0f;
            };
            int cores = 0;
            float sum = 0.0f;
            if (found0) {
                out.cpu0Pct = coreLoad(idle0, prevIdle0);
                sum += out.cpu0Pct;
                ++cores;
            }
            if (found1) {
                out.cpu1Pct = coreLoad(idle1, prevIdle1);
                sum += out.cpu1Pct;
                ++cores;
            }
            if (cores > 0) {
                out.cpuPct = std::roundf(10.0f * (sum / float(cores))) / 10.0f;
                out.cpuValid = true;
            }
        }
    }
    prevTotal = totalRunTime;
    prevIdle0 = idle0;
    prevIdle1 = idle1;
    lastSampleMs = nowMs;
    hasBaseline = true;
    cached = out;
    return cached;
}

void recordLog(const char* message) {
    Serial.println(message);
    const std::lock_guard lock(logMutex);
    const auto id = ++sequence;
    auto& entry = logs[(id - 1) % logs.size()];
    entry.sequence = id;
    entry.clock = clockStamp();
    snprintf(entry.message, sizeof(entry.message), "%s", message);
}

void copyManagementPassword(char (&output)[65]) {
    static_assert(sizeof(output) == sizeof(adminPassword));
    const std::lock_guard lock(adminMutex);
    snprintf(output, sizeof(output), "%s", adminPassword);
}
}
