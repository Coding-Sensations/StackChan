#pragma once
#include <cJSON.h>
#include <string>
#include <cstdio>
#include <cmath>
namespace companion {
inline const char* FeatureScopeLabel(const std::string& cap){
 if(cap=="desktop.health")return "Read PC health";
 if(cap=="network.wol")return "Send wake packet";
 if(cap=="apple.now_playing")return "Read Apple Music";
 if(cap=="apple.home.scenes")return "List Home scenes";
 if(cap=="apple.home.execute")return "Run Home scenes";
 if(cap=="mira.forwarding.metadata")return "Refresh original Mira binding";
 if(cap=="mira.forwarding.status")return "Read Mira forwarding status";
 if(cap=="mira.forwarding.interrupt")return "Stop Mira forwarding";
 return "Unknown capability";
}
inline std::string FeatureResultText(const std::string& raw){
 auto p=cJSON_Parse(raw.c_str());
 if(!p)return raw;
 auto item=[&](cJSON* root,const char* name){return cJSON_GetObjectItemCaseSensitive(root,name);};
 auto text=[&](const char* name){auto v=item(p,name);return std::string(cJSON_IsString(v)&&v->valuestring?v->valuestring:"");};
 std::string out;
 auto error=text("error");
 if(!error.empty()){
  if(error=="notConfigured")out="Host feature is not configured.";else if(error=="permissionRequired")out="Enable this feature on the host.";else if(error=="denied")out="Host permission denied.";
  else if(error=="busy")out="Host action is still pending.";else if(error=="unavailable")out="Feature is currently unavailable.";else out="Host could not complete this request.";
 }else if(auto snapshot=item(p,"snapshot");cJSON_IsObject(snapshot)){
  auto value=[&](const char* key,const char* label,double divisor,const char* unit){auto v=item(snapshot,key);out+=label;char b[64];
 if(cJSON_IsNumber(v)&&std::isfinite(v->valuedouble)){std::snprintf(b,sizeof(b),"%.1f%s",v->valuedouble/divisor,unit);out+=b;}else out+="unavailable";out+='\n';};
  value("cpu_percent","CPU: ",1,"%");value("memory_available_bytes","Memory free: ",1073741824.," GiB");value("disk_available_bytes","Disk free: ",1073741824.," GiB");value("temperature_celsius","Temperature: ",1," C");
 }else if(item(p,"mira")){out=cJSON_IsNull(item(p,"mira"))?"No confirmed native call binding.\nConnect the approved original route, then Refresh.":"Original Mira forwarding binding available.\nStatus or Stop local does not end the call.";
 }else if(cJSON_IsTrue(item(p,"available"))){if(cJSON_IsNull(item(p,"track")))out="No Music track available.";else out=(cJSON_IsTrue(item(p,"playing"))?"Playing: ":"Paused: ")+text("title")+"\n"+text("artist")+"\n"+text("album");
 }else if(cJSON_IsArray(item(p,"scenes")))out=std::to_string(cJSON_GetArraySize(item(p,"scenes")))+" Home scenes available.\nChoose one before Run scene.";
 else if(cJSON_IsTrue(item(p,"executed")))out="Home scene completed.";
 else if(cJSON_IsNumber(item(p,"bytes_sent")))out="Wake packet sent.\nWake is not verified.";
 else if(item(p,"local_forwarding_muted")){
  out=cJSON_IsTrue(item(p,"local_forwarding_muted"))?"Local Mira forwarding muted.":"Local Mira forwarding active.";
  out+="\nOriginal call closure is not confirmed.";
  if(cJSON_IsFalse(item(p,"speaker_stop_confirmed"))||cJSON_IsFalse(item(p,"provider_cancel_confirmed")))out+="\nSpeaker/provider stop is not confirmed.";
 }else out="Host reply received.";
 cJSON_Delete(p);return out;
}
}
