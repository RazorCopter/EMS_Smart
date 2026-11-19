// SPDX-License-Identifier: MIT
#include "m5.h"
#include "clock_service.h"
#include "network/rtc_time.h"
#include "remote.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

namespace heliograph::v51 {
namespace {
std::mutex auxMutex, inputMutex;
bool commanded=false, permitted=false;
uint32_t until=0;
bool timed=false;
const char* lastAuxOrigin=nullptr;
const char* lastAuxAction=nullptr;
const char* lastAuxReason=nullptr;
constexpr auto kAux=GPIO_NUM_47;
struct Input {
    const char* name;
    unsigned ttl;
    time_t timestamp=0, expires=0;
    uint64_t acceptedAt=0;
    uint32_t validityMs=0, initialAge=0;
    std::string payload, brief, source, messageId, quality, units;
    uint32_t accepted=0, rejected=0;
    const char* rejection=nullptr;
};
std::array<Input,5> inputs{{{"forecast_solar",50400},{"forecast_ratio",1800},
    {"weather",1800},{"load_profile",172800},{"heartbeat",90}}};
bool (*authorize)(httpd_req_t*)=nullptr;

void event(const char* origin,const char* command,const char* outcome) {
    if(strncmp(command,"AUX_",4)==0) {
        lastAuxAction=command;lastAuxOrigin=origin;lastAuxReason=outcome;
    }
    char row[190];
    snprintf(row,sizeof(row),"MANUAL origin=%s action=%s outcome=%s AUX1=%s physical_feedback=null",
             origin,command,outcome,commanded?"ON":"OFF");
    recordLog(row);
}
void setAux(bool on) {
    gpio_set_level(kAux,on ? 1 : 0);
    commanded=on;
}
bool range(JsonVariantConst v,double low,double high) {
    if(!v.is<double>()) return false;
    const auto n=v.as<double>();
    return std::isfinite(n) && n>=low && n<=high;
}
// Both wire versions use UTC. Version 2 uses the canonical ISO representation.
int64_t utcEpoch(JsonVariantConst value) {
    if(value.is<int64_t>())return value.as<int64_t>();
    if(!value.is<const char*>())return 0;
    const auto* s=value.as<const char*>();
    if(strlen(s)!=20 || s[4]!='-' || s[7]!='-' || s[10]!='T' || s[13]!=':' ||
       s[16]!=':' || s[19]!='Z')return 0;
    for(int i:{0,1,2,3,5,6,8,9,11,12,14,15,17,18})
        if(s[i]<'0' || s[i]>'9')return 0;
    const auto two=[s](int i){return (s[i]-'0')*10+s[i+1]-'0';};
    const int y=two(0)*100+two(2),m=two(5),d=two(8),h=two(11),n=two(14),sec=two(17);
    if(y<2024 || y>2099 || m<1 || m>12 || d<1 || d>31 || h>23 || n>59 || sec>59)return 0;
    const auto days=rtc::daysFromCivil(y,m,d);
    int checkY,checkM,checkD;
    rtc::civilFromDays(days,checkY,checkM,checkD);
    if(y!=checkY || m!=checkM || d!=checkD)return 0;
    return days*86400+h*3600+n*60+sec;
}
bool forecast(JsonVariantConst v,time_t now) {
    if(!v.is<JsonObjectConst>() || !v["poll_timestamp"].is<int64_t>()) return false;
    auto poll=v["poll_timestamp"].as<int64_t>();
    if(poll>now+5 || poll<=now-50400) return false;
    size_t count=0;
    for(auto key:{"today","tomorrow"}) {
        if(!v[key].is<JsonArrayConst>() || v[key].size()>50) return false;
        int64_t previous=0;
        for(auto row:v[key].as<JsonArrayConst>()) {
            auto start=row["start"].as<int64_t>();
            if(!row["start"].is<int64_t>() || start<=previous || start<now-90000 ||
               start>now+183600 || !range(row["kw"],0,100)) return false;
            previous=start;
            ++count;
        }
    }
    return count!=0;
}
std::string encode(JsonDocument& doc) {
    std::string data;
    serializeJson(doc,data);
    return data;
}
esp_err_t respond(httpd_req_t* request,const std::string& value) {
    httpd_resp_set_type(request,"application/json");
    httpd_resp_set_hdr(request,"Cache-Control","no-store");
    return httpd_resp_send(request,value.data(),value.size());
}
esp_err_t getInputs(httpd_req_t* r) {
    if(!authorize(r)) return ESP_OK;
    return respond(r,haSummary(true));
}
esp_err_t getManual(httpd_req_t* r) {
    if(!authorize(r)) return ESP_OK;
    return respond(r,manualState());
}
esp_err_t postManual(httpd_req_t* r) {
    if(!authorize(r)) return ESP_OK;
    char buffer[220];
    if(!r->content_len || r->content_len>=sizeof(buffer))
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"INVALID");
    size_t received=0;
    while(received<r->content_len) {
        int n=httpd_req_recv(r,buffer+received,r->content_len-received);
        if(n<=0)return ESP_FAIL;
        received+=n;
    }
    JsonDocument request;
    if(deserializeJson(request,buffer,received))
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"INVALID");
    const char* origin=strcmp(r->uri,"/api/v1/manual/ui")==0?"WEB_UI":"REST_API";
    const auto outcome=manualCommand(request["action"]|"",request["payload"],origin);
    JsonDocument ack;
    ack["accepted"]=outcome.accepted;ack["reason"]=outcome.reason;ack["executed"]=false;
    addClock(ack.as<JsonObject>(),clockStamp());
    ack["aux1"]=auxOn()?"ON":"OFF";ack["physical_feedback"]=nullptr;
    if(!outcome.accepted)httpd_resp_set_status(r,"409 Conflict");
    return respond(r,encode(ack));
}
}

void serviceAux(bool healthy) {
    const std::lock_guard guard(auxMutex);
    permitted=healthy;
    if(commanded && (!healthy || (timed && int32_t(millis()-until)>=0))) {
        setAux(false);
        timed=false;
        event(healthy?"TIMER":"SAFETY_ENGINE","AUX_OFF",healthy?"TTL_EXPIRED":"SAFETY_OFF");
    }
}
bool auxOn() {
    const std::lock_guard guard(auxMutex);
    return commanded;
}
ManualResult manualCommand(const char* action,JsonVariantConst payload,const char* origin,unsigned maximumSeconds) {
    const std::lock_guard guard(auxMutex);
    if(strcmp(action,"operating_mode")==0) {
        const char* mode=payload["mode"]|"";
        const bool shadow=strcmp(mode,"SHADOW")==0;
        const char* reason=shadow?"ACCEPTED":strcmp(mode,"LIVE")==0?"LIVE_NOT_QUALIFIED":"INVALID_MODE";
        event(origin,shadow?"SHADOW":strcmp(mode,"LIVE")==0?"LIVE":"INVALID_MODE",reason);
        return {shadow,reason};
    }
    if(strcmp(action,"aux")!=0 || !payload["on"].is<bool>()) {
        event(origin,"INVALID","INVALID");
        return {false,"INVALID"};
    }
    const bool on=payload["on"].as<bool>();
    const auto ttl=payload["auto_off_s"];
    const unsigned seconds=ttl.isNull()?0:ttl.as<unsigned>();
    const char* reason=(!ttl.isNull() && (!ttl.is<unsigned>() || seconds>86400))?"INVALID_TTL":
        on && !permitted?"SAFETY_BLOCKED":"ACCEPTED";
    const bool accepted=strcmp(reason,"ACCEPTED")==0;
    if(accepted) {
        setAux(on);
        const unsigned effective=seconds ? std::min(seconds,maximumSeconds) : maximumSeconds;
        timed=on && (seconds || maximumSeconds<86400);
        until=millis()+1000u*effective;
    }
    event(origin,on?"AUX_ON":"AUX_OFF",reason);
    return {accepted,reason};
}
std::string manualState() {
    const std::lock_guard guard(auxMutex);
    JsonDocument d;
    d["operating_mode"]="SHADOW";d["live_lock"]="LIVE_NOT_QUALIFIED";
    d["aux1"]=commanded?"ON":"OFF";d["physical_feedback"]=nullptr;
    d["last_aux_origin"]=lastAuxOrigin;d["last_aux_action"]=lastAuxAction;d["last_aux_reason"]=lastAuxReason;
    d["aux_auto_off_ms"]=commanded && timed ? std::max<int32_t>(0,int32_t(until-millis())) : 0;
    addClock(d.as<JsonObject>(),clockStamp());
    return encode(d);
}

bool acceptHaInput(const char* kind,const char* data,size_t size) {
    const auto it=std::find_if(inputs.begin(),inputs.end(),[&](const Input& i){return strcmp(i.name,kind)==0;});
    if(it==inputs.end())return false;
    const auto reject=[it](const char* reason){
        const std::lock_guard guard(inputMutex);
        ++it->rejected;it->rejection=reason;
        return false;
    };
    if(!data)return reject("RETAINED");
    JsonDocument d;
    if(!size || size>7500 || deserializeJson(d,data,size))return reject("MALFORMED");
    const int version=d["schema"]|0;
    if(version!=1 && version!=2)return reject("SCHEMA");
    const bool modern=version==2;
    const char* source=d["source"]|"legacy_home_assistant";
    const char* id=d["message_id"]|"";
    if(modern && (strcmp(source,"home_assistant")!=0 || !*id || strlen(id)>80 ||
       !d["timestamp"].is<const char*>() || !d["valid_until"].is<const char*>() ||
       !d["units"].is<JsonObjectConst>() || d["units"].size()>12 ||
       !d["quality"].is<const char*>() || strlen(d["quality"].as<const char*>())>32))
        return reject("SCHEMA");
    const auto clock=clockStamp();
    const auto now=clock.utc;
    const auto ts=utcEpoch(d["timestamp"]), until=utcEpoch(d["valid_until"]);
    if(!now || ts<=0 || ts>now+5)return reject("TIMESTAMP");
    if(until<=ts || until-ts>it->ttl)return reject("VALID_UNTIL");
    if(until<=now)return reject("EXPIRED");
    auto v=d[modern?"payload":"data"].as<JsonVariantConst>();
    bool valid=false;
    if(strcmp(kind,"forecast_solar")==0) valid=forecast(v,now) && until<=v["poll_timestamp"].as<int64_t>()+50400;
    else if(strcmp(kind,"forecast_ratio")==0) valid=range(v["ratio"],0,10) || (modern && v["ratio"].isNull());
    else if(strcmp(kind,"weather")==0)
        valid=(v["fog"].is<bool>() || (modern && v["fog"].isNull())) &&
              (v["spread_c"].isNull() || range(v["spread_c"],-30,100)) &&
              (v["radiation_w_m2"].isNull() || range(v["radiation_w_m2"],0,2000));
    else if(strcmp(kind,"load_profile")==0)
        valid=range(v["today_kw"],0,100) && range(v["tomorrow_kw"],0,100);
    else valid=v["online"]==true;
    if(!valid || !v.is<JsonObjectConst>())return reject("PAYLOAD");
    std::string payload,brief;
    serializeJson(v,payload);
    if(strcmp(kind,"forecast_solar")==0) {
        JsonDocument summary;
        summary["today_periods"]=v["today"].size();summary["tomorrow_periods"]=v["tomorrow"].size();
        summary["today_total_kwh"]=v["today_total_kwh"];
        summary["tomorrow_total_kwh"]=v["tomorrow_total_kwh"];
        serializeJson(summary,brief);
    }
    const std::lock_guard guard(inputMutex);
    if(ts<it->timestamp || (ts==it->timestamp && (!modern || it->messageId==id))) {
        ++it->rejected;it->rejection="DUPLICATE_OR_OLDER";return false;
    }
    it->source=modern?source:"legacy_home_assistant";
    it->messageId=modern?id:"";
    it->quality=modern?d["quality"].as<const char*>():"legacy";
    it->units.clear();
    if(modern)serializeJson(d["units"],it->units);
    ++it->accepted;
    it->timestamp=ts;it->expires=until;
    const auto sourceTime=strcmp(kind,"forecast_solar")==0 ? v["poll_timestamp"].as<int64_t>() : ts;
    it->acceptedAt=esp_timer_get_time()/1000;it->validityMs=uint32_t(until-now)*1000;
    it->initialAge=std::max<int64_t>(0,now-sourceTime);
    it->payload=std::move(payload);it->brief=std::move(brief);
    return true;
}
void rejectRetainedHaInput(const char* kind) {
    acceptHaInput(kind,nullptr,0);
}
std::string haSummary(bool contents, bool dashboard) {
    const std::lock_guard guard(inputMutex);
    const uint64_t now=esp_timer_get_time()/1000;
    JsonDocument d;
    const auto freshAt=[now](const Input& input){return input.timestamp && now-input.acceptedAt<input.validityMs;};
    d["ha_online"]=freshAt(inputs.back());
    for(const auto& input:inputs) {
        auto row=d[input.name].to<JsonObject>();
        const bool fresh=freshAt(input);
        row["status"]=fresh?"valid":input.timestamp?"stale":"unavailable";
        if(input.timestamp) row["age_s"]=input.initialAge+(now-input.acceptedAt)/1000;
        else row["age_s"]=nullptr;
        row["used"]=false;
        row["accepted"]=input.accepted;row["rejected"]=input.rejected;
        row["last_rejection"]=input.rejection;
        if((contents||dashboard) && input.timestamp) {
            addClock(row,{0,input.timestamp,TimeSource::Ntp});
            row.remove("uptime_ms");row.remove("time_source");row.remove("time_valid");
        }
        if(contents||dashboard) {
            row["data"]=nullptr;
            if(fresh) row["data"]=serialized(dashboard && !input.brief.empty()?input.brief:input.payload);
            if(input.expires) {
                auto expiry=row["expiry"].to<JsonObject>();
                addClock(expiry,{0,input.expires,TimeSource::Ntp});
                row["expires_at"]=expiry["timestamp"].as<std::string>();
                row.remove("expiry");
                row["valid_until"]=input.expires;
            } else {
                row["valid_until"]=nullptr;
                row["expires_at"]=nullptr;
            }
            row["source"]=input.source.empty()?nullptr:input.source.c_str();
            row["quality"]=input.quality.empty()?nullptr:input.quality.c_str();
            if(!input.units.empty())row["units"]=serialized(input.units);
            if(!input.messageId.empty())row["message_id"]=input.messageId;
        }
    }
    return encode(d);
}
void registerM5(httpd_handle_t server,bool(*auth)(httpd_req_t*)) {
    authorize=auth;
    const httpd_uri_t routes[]={
        {"/api/v1/manual",HTTP_GET,getManual,nullptr},
        {"/api/v1/manual",HTTP_POST,postManual,nullptr},
        {"/api/v1/manual/ui",HTTP_POST,postManual,nullptr},
        {"/api/v1/ha/inputs",HTTP_GET,getInputs,nullptr}
    };
    for(const auto& r:routes)
        if(httpd_register_uri_handler(server,&r)!=ESP_OK)recordLog("M5 HTTP route registration failed");
}
}
