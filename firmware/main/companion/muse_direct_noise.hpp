// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_session.hpp"
#include <xplat/noise/core/ClientSession.h>
#include <memory>
#include <new>
namespace companion::muse {
namespace wire=musegadgets::noise::core;
// Socket adapter must verify the fixed Meta hostname/CA and bearer upgrade.
// It never retains caller buffers; no received callback can reenter this owner.
// A concrete adapter is supplied only after separate network/runtime approval.
class NoiseSocket {
public:
    virtual ~NoiseSocket()=default;
    virtual bool ConnectMeta(const std::string& exact_vm,const char* bearer,unsigned timeout_ms)=0;
    virtual bool VerifiedMetaEndpoint()const=0;
    virtual bool Send(const uint8_t*,size_t,unsigned timeout_ms)=0;
    virtual int Receive(uint8_t*,size_t,unsigned timeout_ms)=0;
    virtual bool CloseAndJoin(unsigned timeout_ms)=0;
};
class NoiseChatOwner {
    NoiseSocket& socket_;
    std::unique_ptr<wire::ClientSession> session_;
    std::array<uint8_t,wire::ClientSession::kMaxOutboundWebSocketPayloadSize> ws_{};
    std::array<uint8_t,12288> frame_{},response_{};
    std::array<uint8_t,2048> service_{};
    std::array<uint8_t,4096> envelope_{};
    std::array<wire::HeaderView,16> headers_{};
    std::function<bool()> current_;
    std::function<uint64_t()> clock_;
    std::function<void(const std::string&,bool)> caption_;
    std::string node_,chat_,generation_,register_id_,ack_,pending_,control_,events_,ack_body_,assistant_,reply_text_;
    uint64_t deadline_=0,last_=0,sequence_=0;
    bool spent_=false,open_=false,registered_=false,subscribed_=false,submitted_=false,cleanup_=true,ack_response_=false,dialed_=false,reply_done_=false;
    bool Current(){const auto now=clock_?clock_():0;if(!open_||!current_||!current_()||now<last_||now>=deadline_)return false;last_=now;return true;}
    bool Flush(){for(unsigned n=0;session_->HasOutboundWebSocketPayload()&&n<8;++n){if(!Current())return false;auto result=session_->WriteNextOutboundWebSocketPayload(wire::ByteSpan(ws_));if(!result.ok()||!socket_.Send(ws_.data(),result.size(),1000))return false;}return !session_->HasOutboundWebSocketPayload()&&Current();}
    bool Request(int64_t id,const char* path,const std::string& body,bool end=true){
        wire::ApplicationRequestView request;request.verb=wire::StringView("POST");request.path=wire::StringView(path);request.body=wire::ConstByteSpan(reinterpret_cast<const uint8_t*>(body.data()),body.size());request.end_body=end;
        const wire::HeaderView json[]={{wire::StringView("Content-Type"),wire::StringView("application/json")},{wire::StringView("Accept"),wire::StringView("application/x-ndjson")}};request.headers=wire::Span<const wire::HeaderView>(json,!std::strcmp(path,"/chat/subscribe")?2:1);
        return session_->StartOutboundApplicationRequest(wire::ServiceType::Daemon,id,request,wire::ByteSpan(service_),wire::ByteSpan(envelope_)).ok()&&Flush();
    }
    bool Control(const std::string& json){
        if(json.size()>2048)return false;
        std::array<uint8_t,2052> body{};const auto size=json.size();for(unsigned i=0;i<4;++i)body[i]=uint8_t(size>>(8*i));std::memcpy(body.data()+4,json.data(),size);
        wire::BodyChunkView chunk{wire::ConstByteSpan(body.data(),size+4),false};
        return session_->StartOutboundBodyChunk(wire::ServiceType::Daemon,1,chunk,wire::ByteSpan(service_),wire::ByteSpan(envelope_)).ok()&&Flush();
    }
    bool OnControl(cJSON* root){
        if(!Unique(root))return false;
        const auto method=Text(root,"method"),id=Text(root,"id"),event=Text(root,"event");
        if(event=="link.unpaired"||event=="node.unpaired")return false;
        if(id==register_id_&&method.empty()){
            auto* error=cJSON_GetObjectItemCaseSensitive(root,"error");
            if(registered_||(error&&!cJSON_IsNull(error))||cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(root,"ok")))return false;
            registered_=true;
            Json request(cJSON_CreateObject());cJSON_AddStringToObject(request.value,"session_id",chat_.c_str());return Request(3,"/chat/subscribe",Encode(request.value));
        }
        if(method=="link.invoke"){
            if(!Id(id,64))return false;
            Json result(cJSON_CreateObject());cJSON_AddStringToObject(result.value,"method","link.result");cJSON_AddStringToObject(result.value,"id",id.c_str());cJSON_AddBoolToObject(result.value,"ok",false);
            auto* error=cJSON_AddObjectToObject(result.value,"error");cJSON_AddStringToObject(error,"code","unsupported");cJSON_AddStringToObject(error,"message","This device session has no command capability.");
            return Control(Encode(result.value)); // Includes OTA, health, BLE, media and motion; no built-in bypass.
        }
        return true;
    }
    bool ControlData(const uint8_t* data,size_t size){
        if(control_.size()+size>8196)return false;
        control_.append(reinterpret_cast<const char*>(data),size);
        while(control_.size()>=4){uint32_t length=0;for(unsigned i=0;i<4;++i)length|=uint32_t(uint8_t(control_[i]))<<(8*i);if(length>4096)return false;if(control_.size()<length+4)return true;
            std::string body=control_.substr(4,length);control_.erase(0,length+4);if(!length)continue;
            Json root(Parse(body));if(!OnControl(root.value))return false;
        }return true;
    }
    bool Event(cJSON* root){
        if(!Unique(root))return false;
        if(Text(root,"type")!="event")return true;
        auto* seq=cJSON_GetObjectItemCaseSensitive(root,"seq");if(!cJSON_IsNumber(seq)||seq->valuedouble<1||seq->valuedouble>9007199254740991.||uint64_t(seq->valuedouble)!=seq->valuedouble)return false;
        const auto next=uint64_t(seq->valuedouble);if(next<=sequence_)return false;sequence_=next;
        if(!submitted_||ack_.empty())return true; // Ignore history before the exact acknowledgement.
        auto* payload=cJSON_GetObjectItemCaseSensitive(root,"payload");if(!Unique(payload))return true;
        if(reply_done_)return true;
        auto role=Text(payload,"role");if(!role.empty()&&role!="assistant")return true;
        const auto event=Text(root,"event"),mid=Text(payload,"message_id");
        if(!Id(mid,128)||(event!="delta.message_start"&&event!="delta.text_append"&&event!="delta.message_done"))return true;
        auto parent=Text(payload,"reply_to_message_id");if(parent.empty())parent=Text(payload,"parent_message_id");
        if(assistant_.empty()){if(parent!=ack_)return true;assistant_=mid;}
        else if(mid!=assistant_||(!parent.empty()&&parent!=ack_))return true;
        auto text=Text(payload,event=="delta.message_done"?"display_text":"text");
        if(text.empty()&&event=="delta.message_done")text=Text(payload,"content");
        if(event=="delta.text_append")reply_text_+=text;else if(!text.empty())reply_text_=text;
        if(reply_text_.size()>160)reply_text_.resize(160);
        for(char& c:reply_text_)if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>126)c='?';
        reply_done_=event=="delta.message_done";
        if(caption_)caption_(reply_text_,reply_done_);
        return true; // Reply end never asserts task success.
    }

    bool EventData(const uint8_t* data,size_t size){
        if(events_.size()+size>8192)return false;
        events_.append(reinterpret_cast<const char*>(data),size);
        size_t newline=0;while((newline=events_.find('\n'))!=std::string::npos){auto body=events_.substr(0,newline);events_.erase(0,newline+1);if(body.empty())continue;Json root(Parse(body));if(!Event(root.value))return false;}return true;
    }
    bool Inbound(const wire::DecodedServiceFrame& frame){
        const bool response=frame.kind==wire::ServiceFrameKind::Response;
        if(frame.kind!=wire::ServiceFrameKind::Response&&frame.kind!=wire::ServiceFrameKind::BodyChunk)return false;
        const auto body=response?frame.response.body:frame.body_chunk.data;const bool ended=response?frame.response.end_body:frame.body_chunk.end_body;
        if(response&&(frame.response.status<200||frame.response.status>=300))return false;
        if(frame.stream_id==1)return !ended&&ControlData(body.data(),body.size());
        if(frame.stream_id==3){if(!registered_||ended)return false;if(response)subscribed_=true;return EventData(body.data(),body.size());}
        if(frame.stream_id==5){
            if(!submitted_||!ack_.empty()||ack_body_.size()+body.size()>2048)return false;
            if(response)ack_response_=true;
            if(!ack_response_)return false;
            ack_body_.append(reinterpret_cast<const char*>(body.data()),body.size());if(!ended)return true;
            Json root(Parse(ack_body_));Wipe(ack_body_);
            if(!Unique(root.value)||Text(root.value,"session_id")!=chat_||!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root.value,"is_thread")))return false;
            auto id=Text(root.value,"message_id");if(!Id(id,128))return false;
            ack_=id;return true;
        }
        return false;
    }
public:
    explicit NoiseChatOwner(NoiseSocket& socket):socket_(socket){}
    NoiseChatOwner(const NoiseChatOwner&)=delete;
    bool Open(wire::CryptoBackend& crypto,const std::string& vm,const char* token,const std::string& node,const std::string& chat,const std::string& generation,uint64_t deadline,
        std::function<uint64_t()> clock,std::function<bool()> current,std::function<void(const std::string&,bool)> caption){
        if(spent_||!cleanup_||!clock||!current||!current()||!Id(vm)||!Id(node)||!Id(chat)||!Id(generation,64)||!token||!std::strlen(token)||std::strlen(token)>3072)return false;
        const auto now=clock();if(deadline<=now||deadline-now>120000)return false;
        for(const char* p=token;*p;++p)if(*p<33||*p>126)return false;
        spent_=true;cleanup_=false;open_=true;clock_=std::move(clock);current_=std::move(current);caption_=std::move(caption);last_=now;deadline_=deadline;node_=node;chat_=chat;generation_=generation;register_id_="muse-"+generation;
        session_.reset(new(std::nothrow)wire::ClientSession(crypto));
        if(!session_){Close();return false;}
        dialed_=true;
        if(!socket_.ConnectMeta(vm,token,5000)||!socket_.VerifiedMetaEndpoint()||!Current()){Close();return false;}
        auto first=session_->WriteHandshakeMessage1(wire::ByteSpan(ws_));if(!first.ok()||!socket_.Send(ws_.data(),first.size(),1000)){Close();return false;}
        const int count=socket_.Receive(ws_.data(),512,2000);size_t extra_size=0;std::array<uint8_t,256> extra{};
        if(count<=0||count>512||!Current()||!session_->ReadHandshakeMessage2(wire::ConstByteSpan(ws_.data(),count),wire::ByteSpan(extra),extra_size).ok()){Close();return false;}
        const auto third=session_->WriteHandshakeMessage3(wire::ConstByteSpan(),wire::ByteSpan(ws_));
        if(!third.ok()||!socket_.Send(ws_.data(),third.size(),1000)||!Request(1,"/link-control","",false)){Close();return false;}
        Json registration(cJSON_CreateObject());cJSON_AddStringToObject(registration.value,"type","req");cJSON_AddStringToObject(registration.value,"id",register_id_.c_str());cJSON_AddStringToObject(registration.value,"method","link.register");
        auto* params=cJSON_AddObjectToObject(registration.value,"params");cJSON_AddStringToObject(params,"node_id",node_.c_str());cJSON_AddStringToObject(params,"display_name","Muse");cJSON_AddStringToObject(params,"platform","esp32");cJSON_AddStringToObject(params,"version","1");cJSON_AddStringToObject(params,"device_family","link");cJSON_AddStringToObject(params,"model_id","stackchan");cJSON_AddBoolToObject(params,"is_wakeup_supported",false);cJSON_AddObjectToObject(params,"commands_v2");
        if(!Control(Encode(registration.value))){Close();return false;}return true;
    }
    bool Poll(){
        if(!Current()){Close();return false;}
        const int count=socket_.Receive(ws_.data(),ws_.size(),20);if(count<0||size_t(count)>ws_.size()){Close();return false;}if(!count)return true;
        auto result=session_->ProcessInboundWebSocketPayload(wire::ConstByteSpan(ws_.data(),count),wire::ByteSpan(frame_),wire::ByteSpan(response_),wire::Span<wire::HeaderView>(headers_));
        if(!result.ok()||(result.frame_status==wire::InboundFrameStatus::Complete&&!Inbound(result.frame))){Close();return false;}return true;
    }
    bool Submit(const std::string& request,const std::string& text){
        if(!Current()||!registered_||!subscribed_||submitted_||!Id(request,64)||text.empty()||text.size()>256)return false;
        for(char c:text)if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>126)return false;
        submitted_=true;pending_=request;
        Json body(cJSON_CreateObject());cJSON_AddStringToObject(body.value,"message",text.c_str());cJSON_AddStringToObject(body.value,"session_id",chat_.c_str());cJSON_AddStringToObject(body.value,"device_id",node_.c_str());cJSON_AddStringToObject(body.value,"output_modality","text");
        if(!Request(5,"/chat/stream",Encode(body.value))){Close();return false;}return true;
    }
    bool Close(){
        open_=registered_=subscribed_=false;if(caption_)caption_("",false);caption_={};current_={};clock_={};ack_.clear();pending_.clear();assistant_.clear();Wipe(reply_text_);Wipe(control_);Wipe(events_);Wipe(ack_body_);session_.reset();
        for(auto& array:{&frame_,&response_})for(volatile uint8_t& c:*array)c=0;
        for(volatile uint8_t& c:ws_)c=0;
        for(volatile uint8_t& c:service_)c=0;
        for(volatile uint8_t& c:envelope_)c=0;
        cleanup_=!dialed_||socket_.CloseAndJoin(2000);if(cleanup_)dialed_=false;return cleanup_;
    }
    bool Ready()const{return open_&&registered_&&subscribed_;}
    bool CleanupComplete()const{return cleanup_;}
    static constexpr size_t BufferBytes(){return wire::ClientSession::kMaxOutboundWebSocketPayloadSize+12288*2+2048+4096;}
};
}
