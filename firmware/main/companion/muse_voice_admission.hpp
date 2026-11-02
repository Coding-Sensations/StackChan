// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include <cJSON.h>
#include "muse_direct_credentials.hpp"
#include "muse_voice_session_worker.hpp"
namespace companion::muse {
// Captured local scope, not native attestation or microphone/output permission.
// Each callback must validate its exact original actor/node/VM/generation scope.
struct VoiceAdmissionScope {
    std::string node,vm,call;
    muse_native_review::Budget budget{};
    std::function<bool()> credential_current,network_current,voice_current;
};
template<class Lookup,class Socket>
class VoiceAdmission final:public muse_native_review::Custody {
    using Worker=VoiceSessionWorker<Socket>;
    using LookupFactory=std::function<std::unique_ptr<Lookup>(std::function<bool()>)>;
    using SocketFactory=std::function<std::unique_ptr<Socket>(std::function<bool()>)>;
    voice_prep::Protocol& protocol_;DeviceCredentialReader& reader_;musegadgets::noise::core::CryptoBackend& crypto_;
    muse_native_review::Clock& clock_;muse_native_review::Custody& source_;
    LookupFactory make_lookup_;SocketFactory make_socket_;
    std::unique_ptr<Lookup> lookup_;std::unique_ptr<Socket> socket_;
    std::unique_ptr<musegadgets::noise::core::ClientSession> session_;
    std::unique_ptr<Worker> worker_;
    VoiceAdmissionScope scope_{};muse_native_review::Stamp last_{};
    std::array<uint8_t,musegadgets::noise::core::ClientSession::kMaxOutboundWebSocketPayloadSize> frame_{};
    bool spent_=false,active_=false,stop_=false,closed_=false,busy_=false,checking_=false,lookup_joined_=false,socket_joined_=false;
    struct Busy{bool& value;explicit Busy(bool& v):value(v){value=true;}~Busy(){value=false;}};
    VoiceAdmission(voice_prep::Protocol& p,DeviceCredentialReader& r,musegadgets::noise::core::CryptoBackend& crypto,muse_native_review::Clock& clock,muse_native_review::Custody& source,LookupFactory lookup,SocketFactory socket)
        :protocol_(p),reader_(r),crypto_(crypto),clock_(clock),source_(source),make_lookup_(std::move(lookup)),make_socket_(std::move(socket)){}
    static bool Same(const muse_native_review::Budget& a,const muse_native_review::Budget& b){return a.generation==b.generation&&a.expires_us==b.expires_us&&a.expires_wall_ms==b.expires_wall_ms;}
    bool Valid(muse_native_review::Stamp n)const{return n.mono_us>=0&&n.mono_us<=INT64_MAX-240000000&&n.wall_ms>=1704067200000LL&&n.wall_ms<=INT64_MAX-240000&&
        n.mono_us>=last_.mono_us&&n.wall_ms>=last_.wall_ms&&scope_.budget.expires_us>n.mono_us&&scope_.budget.expires_wall_ms>n.wall_ms&&
        scope_.budget.expires_us-n.mono_us<=120000000&&scope_.budget.expires_wall_ms-n.wall_ms<=120000;}
    bool CurrentGrant(){
        if(!active_||stop_||closed_||checking_)return false;
        Busy checking(checking_);muse_native_review::Stamp before{},after{};muse_native_review::Budget fresh{};
        if(!clock_.Sample(before)||stop_||!Valid(before)||!source_.ReadBudget(fresh)||stop_||!Same(fresh,scope_.budget)||!source_.Current(scope_.budget)||stop_||
           !clock_.Sample(after)||stop_||after.mono_us<before.mono_us||after.wall_ms<before.wall_ms||!Valid(after)||
           !source_.ReadBudget(fresh)||stop_||!Same(fresh,scope_.budget)||!scope_.credential_current()||stop_||!scope_.network_current()||stop_||
           !scope_.voice_current()||stop_||!source_.Current(scope_.budget)||stop_)return false;
        last_=after;return true;
    }
    void WipeFrame(){for(volatile auto& c:frame_)c=0;}
    bool Fail(){active_=false;stop_=true;WipeFrame();return false;}
    unsigned Timeout(unsigned max)const{return static_cast<unsigned>(std::min<int64_t>(max,(scope_.budget.expires_us-last_.mono_us)/1000));}
    bool PeerCurrent(){return CurrentGrant()&&socket_->VerifiedMetaEndpoint()&&CurrentGrant();}
    bool Send(size_t size){
        if(!size||size>frame_.size()||!PeerCurrent()||!Timeout(1000)){WipeFrame();return false;}
        const bool sent=socket_->Send(frame_.data(),size,Timeout(1000));WipeFrame();return sent&&PeerCurrent();
    }
    static bool Metadata(const VoiceAdmissionScope& s){
        return s.call.size()==36&&voice_prep::Protocol::ValidAdmissionIds(s.call,s.node)&&Id(s.vm)&&s.budget.generation&&
            s.credential_current&&s.network_current&&s.voice_current;
    }

public:
    static std::unique_ptr<VoiceAdmission> Create(voice_prep::Protocol& p,DeviceCredentialReader& reader,musegadgets::noise::core::CryptoBackend& crypto,muse_native_review::Clock& clock,muse_native_review::Custody& source,LookupFactory lookup,SocketFactory socket){
        if(!lookup||!socket)return {};
        return std::unique_ptr<VoiceAdmission>(new(std::nothrow)VoiceAdmission(p,reader,crypto,clock,source,std::move(lookup),std::move(socket)));
    }
    VoiceAdmission(const VoiceAdmission&)=delete;VoiceAdmission& operator=(const VoiceAdmission&)=delete;
    ~VoiceAdmission(){assert(!spent_||closed_);WipeFrame();} // never performs native IO
    bool ReadBudget(muse_native_review::Budget& out)override{if(!CurrentGrant())return false;out=scope_.budget;return true;}
    bool Current(const muse_native_review::Budget& budget)override{return Same(budget,scope_.budget)&&CurrentGrant();}
    bool Open(const VoiceAdmissionScope& scope){
        if(busy_||spent_||closed_)return false;
        Busy busy(busy_);spent_=true;
        if(!Metadata(scope))return Fail();
        scope_=scope;active_=true;
        if(!CurrentGrant())return Fail();
        // Known concrete factories only construct objects. No constructor IO.
        lookup_=make_lookup_([this]{return CurrentGrant();});socket_=make_socket_([this]{return CurrentGrant();});
        if(!lookup_||!socket_||!CurrentGrant())return Fail();
        std::unique_ptr<DeviceRecord> record(new(std::nothrow)DeviceRecord());std::unique_ptr<VmCredential> vm(new(std::nothrow)VmCredential());
        if(!record||!vm||!reader_.LoadAccess(scope_.node,[this]{return CurrentGrant();},*record)||record->node!=scope_.node||!record->access.Size()||!CurrentGrant())return Fail();
        record->refresh.Clear();
        const bool found=lookup_->FindExactVm(*record,scope_.vm,*vm);record->Clear();
        if(!found||vm->vm!=scope_.vm||!vm->bearer.Size()||!CurrentGrant())return Fail();
        lookup_joined_=lookup_->CloseAndJoin(20);
        if(!lookup_joined_||!CurrentGrant())return Fail();
        session_.reset(new(std::nothrow)musegadgets::noise::core::ClientSession(crypto_));
        if(!session_||!CurrentGrant()||!Timeout(5000))return Fail();
        const bool connected=socket_->ConnectMeta(scope_.vm,vm->bearer.Get(),Timeout(5000));vm->Clear();
        if(!connected||!PeerCurrent())return Fail();
        auto first=session_->WriteHandshakeMessage1(musegadgets::noise::core::ByteSpan(frame_));
        if(!first.ok()||!Send(first.size())||!PeerCurrent()||!Timeout(2000))return Fail();
        const int count=socket_->Receive(frame_.data(),512,Timeout(2000));std::array<uint8_t,256> extra{};size_t extra_size=0;
        const bool second=count>0&&count<=512&&PeerCurrent()&&session_->ReadHandshakeMessage2(musegadgets::noise::core::ConstByteSpan(frame_.data(),static_cast<size_t>(count)),musegadgets::noise::core::ByteSpan(extra),extra_size).ok();
        WipeFrame();for(volatile auto& c:extra)c=0;
        if(!second||!PeerCurrent())return Fail();
        const auto third=session_->WriteHandshakeMessage3(musegadgets::noise::core::ConstByteSpan(),musegadgets::noise::core::ByteSpan(frame_));
        if(!third.ok()||!Send(third.size())||!session_->isEstablished()||!CurrentGrant())return Fail();
        worker_=Worker::Adopt(protocol_,session_,socket_,clock_,*this);
        if(!worker_||!CurrentGrant()||!worker_->Begin({scope_.budget.generation,scope_.call,scope_.node,7,9,scope_.budget.expires_us}))return Fail();
        return true;
    }
    bool Tick(){if(busy_||!active_||stop_||!worker_)return false;Busy busy(busy_);return worker_->Tick()||Fail();}
    bool Cancel(){if(busy_){stop_=true;return false;}if(!worker_||!active_||stop_)return false;Busy busy(busy_);return worker_->Cancel()||Fail();}
    muse_native_review::Bridge* Review(){return worker_&&active_&&!stop_?&worker_->Review():nullptr;}
    bool Close(){
        if(closed_)return true;
        if(busy_){stop_=true;return false;}
        Busy busy(busy_);Fail();
        if(worker_)socket_joined_=worker_->Close();
        else if(!socket_joined_)socket_joined_=!socket_||socket_->CloseAndJoin(20);
        if(!lookup_joined_)lookup_joined_=!lookup_||lookup_->CloseAndJoin(20);
        if(!socket_joined_||!lookup_joined_)return false;
        // Keep the revoked review bridge alive through Close; caller drains UI
        // references before destroying this admission owner.
        session_.reset();socket_.reset();lookup_.reset();
        Wipe(scope_.node);Wipe(scope_.vm);Wipe(scope_.call);scope_={};make_lookup_={};make_socket_={};closed_=true;return true;
    }
    bool CleanupComplete()const{return closed_;}
};
}
#endif
