// SPDX-License-Identifier: MIT
#include "clock_service.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <mutex>

#include "boards/waveshare_esp32_s3_relay_1ch.h"
#include "network/rtc_time.h"
#include "remote.h"
#include "iana_zones.h"

namespace heliograph::v51 {
namespace {
constexpr uint32_t kSyncIntervalMs=6u*60*60*1000;
constexpr time_t kFirstEpoch=1704067200; // 2024-01-01 UTC
struct Config {
    bool enabled=true;
    char zone[64]="Europe/Rome";
    char primary[64]="pool.ntp.org";
    char fallback1[64]="time.cloudflare.com";
    char fallback2[64]="time.google.com";
};
Config settings, active;
Preferences* storage=nullptr;
bool (*authorized)(httpd_req_t*)=nullptr;
std::mutex configLock, syncLock;
std::atomic<TimeSource> source{TimeSource::Unsynced};
std::atomic<bool> listening{false};
bool requested=true, pending=false;
ClockStamp lastSync{}, toWrite{};
time_t savedSync=0;
TimeSource bootSource=TimeSource::Unsynced;
const char* bootRtc="not_read";
const char* rtcWrite="not_attempted";

const char* rules(const char* zone) {
    const auto it=std::lower_bound(std::begin(kZones),std::end(kZones),zone,
        [](const Zone& z,const char* key){return strcmp(z.name,key)<0;});
    return it!=std::end(kZones) && strcmp(it->name,zone)==0 ? it->rule : nullptr;
}
bool sane(time_t utc) { return utc>=kFirstEpoch && static_cast<int64_t>(utc)<4102444800LL; }
bool validServer(const char* server) {
    size_t label=0;
    const char* p=server;
    for(;*p;++p) {
        const unsigned char c=*p;
        if(c=='.') {
            if(!label || p[-1]=='-')return false;
            label=0;
        } else {
            if(!(std::isalnum(c) || (c=='-' && label)))return false;
            ++label;
        }
    }
    return label && p[-1]!='-';
}
bool valid(const Config& c) {
    return rules(c.zone) && validServer(c.primary) && validServer(c.fallback1) &&
        validServer(c.fallback2);
}
const char* readRtc(time_t& utc) {
    Wire.beginTransmission(board::kRtcI2cAddress);
    Wire.write(0x04);
    if(Wire.endTransmission(false)!=0 || Wire.requestFrom(board::kRtcI2cAddress,7)!=7)
        return "i2c_unavailable";
    rtc::Registers bytes{};
    for(auto& byte:bytes)byte=static_cast<uint8_t>(Wire.read());
    if(bytes[0]&0x80)return "oscillator_stopped";
    return rtc::decodeRegisters(bytes,utc) && sane(utc) ? "valid":"invalid";
}
bool writeRtc(time_t utc) {
    rtc::Registers bytes{};
    rtc::encodeRegisters(utc,bytes);
    Wire.beginTransmission(board::kRtcI2cAddress);
    Wire.write(0x04);
    Wire.write(bytes.data(),bytes.size());
    if(Wire.endTransmission()!=0)return false;
    time_t roundtrip{};
    return strcmp(readRtc(roundtrip),"valid")==0 && roundtrip>=utc && roundtrip<=utc+2;
}
void onSync(timeval* utc) {
    if(!utc || !sane(utc->tv_sec) || !listening.load())return;
    const std::lock_guard guard(syncLock);
    lastSync={millis(),utc->tv_sec,TimeSource::Ntp};
    toWrite=lastSync;
    pending=false;
    source.store(TimeSource::Ntp);
}
void iso8601(time_t utc,char (&out)[21]) {
    tm value{};
    gmtime_r(&utc,&value);
    strftime(out,sizeof(out),"%Y-%m-%dT%H:%M:%SZ",&value);
}
esp_err_t send(httpd_req_t* r,JsonDocument& doc) {
    std::string data;
    serializeJson(doc,data);
    httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,data.data(),data.size());
}
esp_err_t getTime(httpd_req_t* r) {
    if(!authorized(r))return ESP_OK;
    JsonDocument doc;
    {
        const std::lock_guard configGuard(configLock);
        doc["enabled"]=settings.enabled;doc["zone"]=settings.zone;
        doc["primary"]=settings.primary;doc["fallback1"]=settings.fallback1;doc["fallback2"]=settings.fallback2;
        doc["boot_source"]=timeSourceName(bootSource);doc["rtc_boot_status"]=bootRtc;doc["rtc_write"]=rtcWrite;
        const std::lock_guard syncGuard(syncLock);
        doc["sync_state"]=!settings.enabled?"disabled":pending?"waiting":
            lastSync.utc?"synchronized":listening.load()?"waiting":"offline";
        if(lastSync.utc) {
            auto row=doc["last_sync"].to<JsonObject>();
            addClock(row,lastSync);
            row["boot_current"]=true;
        } else if(savedSync) {
            auto row=doc["last_sync"].to<JsonObject>();
            addClock(row,{0,savedSync,TimeSource::Ntp});
            row["uptime_ms"]=nullptr;
            row["boot_current"]=false;
        } else doc["last_sync"]=nullptr;
    }
    addClock(doc.as<JsonObject>(),clockStamp());
    return send(r,doc);
}
bool readSetting(JsonDocument& doc,const char* key,char (&out)[64]) {
    if(!doc[key].is<const char*>())return false;
    const auto value=doc[key].as<JsonString>();
    if(value.size()==0 || value.size()>=sizeof(out) || strlen(value.c_str())!=value.size())return false;
    strcpy(out,value.c_str());
    return true;
}
esp_err_t setTimeConfig(httpd_req_t* r) {
    if(!authorized(r))return ESP_OK;
    char body[448]{};
    if(!r->content_len || r->content_len>=sizeof(body))
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid time configuration");
    size_t offset=0;
    while(offset<r->content_len) {
        const int received=httpd_req_recv(r,body+offset,r->content_len-offset);
        if(received<=0)return ESP_FAIL;
        offset+=received;
    }
    JsonDocument doc;
    Config next;
    if(deserializeJson(doc,body,offset) || !doc["enabled"].is<bool>() ||
       !readSetting(doc,"zone",next.zone) || !readSetting(doc,"primary",next.primary) ||
       !readSetting(doc,"fallback1",next.fallback1) || !readSetting(doc,"fallback2",next.fallback2) ||
       !valid(next))
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Unsupported IANA zone or invalid NTP server");
    next.enabled=doc["enabled"].as<bool>();
    {
        const std::lock_guard guard(configLock);
        if(storage->putBytes("time_cfg", &next,sizeof(next))!=sizeof(next))
            return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Time configuration persistence failed");
        settings=next;
        requested=true;
    }
    return getTime(r);
}
esp_err_t syncNow(httpd_req_t* r) {
    if(!authorized(r))return ESP_OK;
    {
        const std::lock_guard guard(configLock);
        if(!settings.enabled) {
            httpd_resp_set_status(r,"409 Conflict");
            return httpd_resp_sendstr(r,"NTP is disabled");
        }
        requested=true;
    }
    JsonDocument result;
    result["requested"]=true;
    return send(r,result);
}
}

const char* timeSourceName(TimeSource value) {
    switch(value) {
        case TimeSource::Rtc:return "RTC";
        case TimeSource::Ntp:return "NTP";
        default:return "UNSYNCED";
    }
}
ClockStamp clockStamp() {
    const auto origin=source.load();
    const auto wall=origin==TimeSource::Unsynced?time_t{}:time(nullptr);
    return {millis(),sane(wall)?wall:0,sane(wall)?origin:TimeSource::Unsynced};
}
void addClock(JsonObject json,ClockStamp stamp) {
    if(stamp.utc) {
        char utc[21]{};
        iso8601(stamp.utc,utc);
        json["timestamp"]=std::string(utc);
    } else json["timestamp"]=nullptr;
    json["uptime_ms"]=stamp.uptime;
    json["time_source"]=timeSourceName(stamp.source);
    json["time_valid"]=stamp.utc!=0;
}
void appendClock(std::ostream& out,ClockStamp stamp) {
    out<<",\"timestamp\":";
    if(stamp.utc) {
        char utc[21]{};
        iso8601(stamp.utc,utc);
        out<<'"'<<utc<<'"';
    } else out<<"null";
    out<<",\"uptime_ms\":"<<stamp.uptime<<",\"time_source\":\""<<timeSourceName(stamp.source)
       <<"\",\"time_valid\":"<<(stamp.utc?"true":"false");
}

void loadClock(Preferences& preferences) {
    storage=&preferences;
    const auto previous=static_cast<time_t>(storage->getLong64("last_ntp_utc",0));
    if(sane(previous))savedSync=previous;
    Config saved;
    if(preferences.getBytesLength("time_cfg")==sizeof(saved) &&
       preferences.getBytes("time_cfg",&saved,sizeof(saved))==sizeof(saved)) {
        for(auto value:{saved.zone,saved.primary,saved.fallback1,saved.fallback2})value[63]='\0';
        if(valid(saved))settings=saved;
    }
    setenv("TZ",rules(settings.zone),1);
    tzset();
    time_t utc{};
    bootRtc=readRtc(utc);
    if(utc && sane(utc) && strcmp(bootRtc,"valid")==0) {
        const timeval value{utc,0};
        if(settimeofday(&value,nullptr)==0)bootSource=TimeSource::Rtc;
    }
    source.store(bootSource);
    sntp_set_time_sync_notification_cb(onSync);
    sntp_set_sync_interval(kSyncIntervalMs);
}
bool registerClock(httpd_handle_t server,bool (*authorize)(httpd_req_t*)) {
    authorized=authorize;
    const httpd_uri_t routes[]={
        {"/api/v1/time",HTTP_GET,getTime,nullptr},
        {"/api/v1/time",HTTP_POST,setTimeConfig,nullptr},
        {"/api/v1/time/sync",HTTP_POST,syncNow,nullptr}
    };
    for(const auto& route:routes)
        if(httpd_register_uri_handler(server,&route)!=ESP_OK)return false;
    return true;
}
void serviceClock(bool wifiConnected) {
    static bool previousWifi=false;
    const bool changedWifi=previousWifi!=wifiConnected;
    previousWifi=wifiConnected;
    {
        const std::lock_guard guard(configLock);
        if(requested || changedWifi) {
            requested=false;
            listening=false;
            if(esp_sntp_enabled())esp_sntp_stop();
            active=settings;
            if(active.enabled && wifiConnected) {
                {
                    const std::lock_guard syncGuard(syncLock);
                    pending=true;
                }
                listening=true;
                configTzTime(rules(active.zone),active.primary,active.fallback1,active.fallback2);
            } else {
                setenv("TZ",rules(active.zone),1);
                tzset();
                const std::lock_guard syncGuard(syncLock);
                pending=false;
            }
        }
    }
    ClockStamp write{};
    {
        const std::lock_guard guard(syncLock);
        std::swap(write,toWrite);
    }
    if(write.utc) {
        const bool ok=writeRtc(time(nullptr));
        bool stored=true;
        {
            const std::lock_guard guard(configLock);
            rtcWrite=ok?"verified":"failed";
            if(savedSync!=write.utc) {
                stored=storage->putLong64("last_ntp_utc",write.utc)==sizeof(int64_t);
                if(stored)savedSync=write.utc;
            }
        }
        if(!stored)recordLog("NTP last synchronization persistence failed");
        recordLog(ok?"NTP synchronized; RTC write verified":"NTP synchronized; RTC write failed");
    }
}
}
