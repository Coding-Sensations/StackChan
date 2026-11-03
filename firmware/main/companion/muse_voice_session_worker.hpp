// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include <voice/service_exchange.hpp>
#include <voice/review_bridge.hpp>
#include <memory>
#include <new>
#include <cassert>
#include <algorithm>
namespace companion::muse {
// Runs serialized on the retained native owner's worker. Adopts exclusive
// already-established SDK/socket custody, not credentials, native rights or a
// verified actor. The actual admission owner supplies Clock/Custody and keeps
// the SDK crypto backend and Protocol alive through checked close/join.
template<class Socket>
class VoiceSessionWorker {
    std::unique_ptr<musegadgets::noise::core::ClientSession> session_;
    std::unique_ptr<Socket> socket_;
    voice_prep::Protocol& protocol_;
    muse_native_review::Clock& clock_;
    muse_native_review::Custody& custody_;
    voice_prep::ServiceExchange exchange_;
    muse_native_review::Bridge review_;
    muse_native_review::Budget budget_{};
    muse_native_review::Stamp last_{};
    std::array<uint8_t,musegadgets::noise::core::ClientSession::kMaxOutboundWebSocketPayloadSize> incoming_{};
    size_t incoming_size_=0;
    bool pinned_=false,begun_=false,spent_=false,outbound_=false,retained_=false,closed_=false,busy_=false,stop_=false;
    struct Busy {bool& value;explicit Busy(bool& v):value(v){value=true;}~Busy(){value=false;}};
    VoiceSessionWorker(voice_prep::Protocol& p,std::unique_ptr<musegadgets::noise::core::ClientSession>& s,std::unique_ptr<Socket>& socket,muse_native_review::Clock& clock,muse_native_review::Custody& custody)
        :session_(std::move(s)),socket_(std::move(socket)),protocol_(p),clock_(clock),custody_(custody),exchange_(p,*session_),review_(p,clock,custody){}
    static bool Same(const muse_native_review::Budget& a,const muse_native_review::Budget& b){return a.generation==b.generation&&a.expires_us==b.expires_us&&a.expires_wall_ms==b.expires_wall_ms;}
    bool Valid(muse_native_review::Stamp now)const{return now.mono_us>=0&&now.wall_ms>=0&&now.mono_us>=last_.mono_us&&now.wall_ms>=last_.wall_ms&&
        budget_.expires_us>now.mono_us&&budget_.expires_wall_ms>now.wall_ms&&budget_.expires_us-now.mono_us<=120000000&&budget_.expires_wall_ms-now.wall_ms<=120000;}
    bool Verify(){
        if(!pinned_||stop_||closed_||retained_)return false;
        muse_native_review::Stamp before{},after{};muse_native_review::Budget fresh{};
        if(!clock_.Sample(before)||stop_||!Valid(before)||!custody_.ReadBudget(fresh)||stop_||!Same(fresh,budget_)||
           !custody_.Current(budget_)||stop_||!socket_->VerifiedMetaEndpoint()||stop_||!clock_.Sample(after)||stop_||
           after.mono_us<before.mono_us||after.wall_ms<before.wall_ms||!Valid(after)||
           !custody_.ReadBudget(fresh)||stop_||!Same(fresh,budget_)||!custody_.Current(budget_)||stop_)return false;
        last_=after;return true;
    }
    void WipeInput(){for(volatile auto& byte:incoming_)byte=0;incoming_size_=0;}
    bool Retain(){retained_=true;stop_=true;review_.Revoke();if(!exchange_.CleanupComplete())exchange_.Abort();WipeInput();return false;}
    unsigned Timeout(unsigned maximum)const{return static_cast<unsigned>(std::min<int64_t>(maximum,(budget_.expires_us-last_.mono_us)/1000));}
public:
    // Failure leaves both caller pointers unchanged. Success transfers cleanup
    // obligation even if Begin is later refused: Close must still complete.
    static std::unique_ptr<VoiceSessionWorker> Adopt(voice_prep::Protocol& p,std::unique_ptr<musegadgets::noise::core::ClientSession>& session,std::unique_ptr<Socket>& socket,muse_native_review::Clock& clock,muse_native_review::Custody& custody){
        if(!session||!socket||!session->isEstablished()||session->HasOutboundWebSocketPayload())return {};
        return std::unique_ptr<VoiceSessionWorker>(new(std::nothrow)VoiceSessionWorker(p,session,socket,clock,custody));
    }
    VoiceSessionWorker(const VoiceSessionWorker&)=delete;
    VoiceSessionWorker& operator=(const VoiceSessionWorker&)=delete;
    ~VoiceSessionWorker(){assert(closed_);WipeInput();} // no implicit native IO
    bool Begin(voice_prep::Scope scope){
        if(busy_||spent_||closed_||retained_)return false;
        Busy busy(busy_);spent_=true;
        if(!custody_.ReadBudget(budget_)||stop_||budget_.generation!=scope.generation||budget_.expires_us!=scope.expires_us)return Retain();
        pinned_=true;
        if(!Verify()||!exchange_.Begin(scope,last_.mono_us,last_.wall_ms))return Retain();
        begun_=true;return true;
    }
    // At most one original-socket write and one read per tick. A nonce-advanced
    // write is acknowledged once, after full success and renewed currentness.
    bool Tick(){
        if(busy_||!begun_||closed_||retained_)return false;
        Busy busy(busy_);
        if(!Verify()||!protocol_.Poll(budget_.generation,last_.mono_us)||exchange_.NeedsTransportClose())return Retain();
        if(!outbound_&&protocol_.Pending().kind!=voice_prep::Kind::None){if(!exchange_.Queue(last_.mono_us))return Retain();outbound_=true;}
        if(outbound_){
            const auto payload=exchange_.Next(last_.mono_us);
            if(!payload.size||!payload.ticket||!Verify()||!Timeout(100))return Retain();
            const bool sent=socket_->Send(payload.data,payload.size,Timeout(100));
            const bool current=Verify();
            if(!exchange_.Written(payload.ticket,sent&&current,last_.mono_us))return Retain();
            outbound_=session_->HasOutboundWebSocketPayload();
        }
        if(!incoming_size_){
            if(!Verify()||!Timeout(20))return Retain();
            const int count=socket_->Receive(incoming_.data(),incoming_.size(),Timeout(20));
            if(count<0||static_cast<size_t>(count)>incoming_.size()||!Verify())return Retain();
            incoming_size_=static_cast<size_t>(count);
        }
        if(incoming_size_){
            const auto result=exchange_.Receive(incoming_.data(),incoming_size_,last_.mono_us);
            if(result==voice_prep::ReceiveResult::Failed)return Retain();
            if(result==voice_prep::ReceiveResult::Consumed)WipeInput(); // Busy retains these exact bytes; no next read.
        }
        if(exchange_.NeedsTransportClose())return Retain();
        return true;
    }
    bool Cancel(){
        if(busy_){stop_=true;return false;}
        if(!begun_||closed_||retained_)return false;
        Busy busy(busy_);
        if(!Verify()||!exchange_.Cancel(last_.mono_us))return Retain();
        return true;
    }
    bool Close(){
        if(closed_)return true;
        if(busy_){stop_=true;return false;} // an in-flight callback cannot retire its own SDK object
        Busy busy(busy_);Retain();
        // Cleanup remains bound to the original port after authorization ends.
        // A failed native join retains session/storage; no release or reconnect.
        if(!socket_->CloseAndJoin(20))return false;
        session_.reset(); // original SDK/crypto state retired before exchange storage release
        if(!exchange_.CleanupComplete()&&!exchange_.Joined(true))return false;
        closed_=true;return true;
    }
    bool ReadCurrentTarget(voice_prep::TargetSnapshot& out){
        out={};if(busy_||!begun_||retained_||closed_)return false;
        Busy busy(busy_);
        if(!Verify()||!protocol_.ReturnedTarget(budget_.generation,last_.mono_us,out)||!Verify()){out={};return false;}return true;
    }
    bool CleanupComplete()const{return closed_;}
    muse_native_review::Bridge& Review(){return review_;} // local projection only; NativePermit remains false
};
}
#endif
