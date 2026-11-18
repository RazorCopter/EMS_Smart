// SPDX-License-Identifier: MIT
#include "m5.h"
#include "remote.h"
#include <Arduino.h>
#include <driver/gpio.h>
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
constexpr auto kAux=GPIO_NUM_47;
struct Input {
    const char* name;
    unsigned ttl;
    time_t timestamp=0, expires=0, sourceTime=0;
    std::string payload;
};
std::array<Input,5> inputs{{{"forecast_solar",50400},{"forecast_ratio",1800},
    {"weather",1800},{"load_profile",172800},{"heartbeat",90}}};
bool (*authorize)(httpd_req_t*)=nullptr;

void event(const char* origin,const char* command,const char* outcome) {
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
bool forecast(JsonVariantConst v) {
    if(!v.is<JsonObjectConst>() || !v["poll_timestamp"].is<int64_t>()) return false;
    auto now=time(nullptr), poll=v["poll_timestamp"].as<int64_t>();
    if(poll>now+5 || now-poll>=50400) return false;
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
    d["aux_auto_off_ms"]=commanded && timed ? std::max<int32_t>(0,int32_t(until-millis())) : 0;
    return encode(d);
}

bool acceptHaInput(const char* kind,const char* data,size_t size) {
    if(!size || size>7500) return false;
    const auto it=std::find_if(inputs.begin(),inputs.end(),[&](const Input& i){return strcmp(i.name,kind)==0;});
    if(it==inputs.end())return false;
    JsonDocument d;
    if(deserializeJson(d,data,size) || d["schema"]!=1 || !d["timestamp"].is<int64_t>() ||
       !d["valid_until"].is<int64_t>())return false;
    auto now=time(nullptr);
    const auto ts=d["timestamp"].as<int64_t>(), until=d["valid_until"].as<int64_t>();
    if(now<1700000000 || ts<=0 || ts>now+5 || until<=now || until<=ts || until-ts>it->ttl) return false;
    auto v=d["data"].as<JsonVariantConst>();
    bool valid=false;
    if(strcmp(kind,"forecast_solar")==0) valid=forecast(v) && until<=v["poll_timestamp"].as<int64_t>()+50400;
    else if(strcmp(kind,"forecast_ratio")==0) valid=range(v["ratio"],0,10);
    else if(strcmp(kind,"weather")==0)
        valid=v["fog"].is<bool>() && (v["spread_c"].isNull() || range(v["spread_c"],-30,100)) &&
              (v["radiation_w_m2"].isNull() || range(v["radiation_w_m2"],0,2000));
    else if(strcmp(kind,"load_profile")==0)
        valid=range(v["today_kw"],0,100) && range(v["tomorrow_kw"],0,100);
    else valid=v["online"]==true;
    if(!valid)return false;
    std::string payload;
    serializeJson(v,payload);
    const std::lock_guard guard(inputMutex);
    if(ts<it->timestamp)return false;
    it->timestamp=ts;it->expires=until;
    it->sourceTime=strcmp(kind,"forecast_solar")==0 ? v["poll_timestamp"].as<int64_t>() : ts;
    it->payload=std::move(payload);
    return true;
}
std::string haSummary(bool contents) {
    const auto now=time(nullptr);
    const std::lock_guard guard(inputMutex);
    JsonDocument d;
    d["ha_online"]=now>=1700000000 && inputs.back().expires>now;
    for(const auto& input:inputs) {
        auto row=d[input.name].to<JsonObject>();
        const bool fresh=now>=1700000000 && input.expires>now && input.timestamp<=now+5;
        row["status"]=fresh?"valid":input.timestamp?"stale":"unavailable";
        if(input.timestamp && now>=1700000000) row["age_s"]=std::max<int64_t>(0,now-input.sourceTime);
        else row["age_s"]=nullptr;
        row["used"]=false;
        if(contents) {
            row["data"]=nullptr;
            if(fresh) row["data"]=serialized(input.payload);
            row["valid_until"]=input.expires;
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
