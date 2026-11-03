#pragma once
#include <apps/stock_shell/named_ir_library.hpp>
namespace stock_tools {
class StateBankFiles:public IrBankFiles {public:virtual std::string Identity() const=0;};
template<class Payload>
struct StateSnapshot {
    uint32_t generation=0;
    Payload library;
    std::string Encode() const {
        const auto payload=library.Export();
        auto body=std::to_string(generation)+"\n"+std::to_string(payload.size())+"\n"+payload;
        return "STACKCHAN_STATE_BANK_V1\n"+std::to_string(IrCrc32(body))+"\n"+body;
    }
    bool Decode(std::string_view bytes) {
        if(bytes.size()>Payload::MaxArchive+128)return false;
        size_t cursor=0;
        auto line=[&](std::string_view& value){auto end=bytes.find('\n',cursor);if(end==std::string_view::npos)return false;value=bytes.substr(cursor,end-cursor);cursor=end+1;return true;};
        auto number=[](std::string_view v,uint32_t& n){if(v.empty() || v.front()<'0' || v.front()>'9')return false;auto r=std::from_chars(v.data(),v.data()+v.size(),n);return r.ec==std::errc{} && r.ptr==v.data()+v.size();};
        std::string_view value;uint32_t checksum=0,gen=0,length=0;
        if(!line(value) || value!="STACKCHAN_STATE_BANK_V1" || !line(value) || !number(value,checksum))return false;
        const auto body=bytes.substr(cursor);
        if(checksum!=IrCrc32(body) || !line(value) || !number(value,gen) || !gen || !line(value) || !number(value,length) || length>Payload::MaxArchive || bytes.size()-cursor!=length)return false;
        Payload candidate;
        if(!candidate.Import(bytes.substr(cursor)))return false;
        generation=gen;library=std::move(candidate);return true;
    }
};
template<class Payload>
class PersistentState {
    StateBankFiles& files_;
    Payload library_;
    uint32_t generation_=0;
    unsigned active_=0;
    IrStoreStatus status_=IrStoreStatus::Unavailable;
    bool writable_=false;
    std::string identity_;
    struct Fingerprint {bool exists=false;size_t size=0;uint32_t crc=0;};
    std::array<Fingerprint,2> loaded_{};
    bool SameStorage() {
        const auto current_identity=files_.Identity();
        if(current_identity.empty() || current_identity!=identity_){status_=IrStoreStatus::StorageChanged;writable_=false;return false;}
        for(unsigned bank=0;bank<2;++bank){
            const bool exists=files_.Exists(bank);
            if(exists!=loaded_[bank].exists){status_=IrStoreStatus::StorageChanged;writable_=false;return false;}
            if(!exists)continue;
            std::string bytes;
            if(!files_.Read(bank,bytes,Payload::MaxArchive+128)){status_=IrStoreStatus::ReadFailed;writable_=false;return false;}
            if(bytes.size()!=loaded_[bank].size || IrCrc32(bytes)!=loaded_[bank].crc){status_=IrStoreStatus::StorageChanged;writable_=false;return false;}
        }
        return true;
    }
    bool Commit(const Payload& candidate) {
        if(!writable_)return false;
        Payload checked;if(!checked.Import(candidate.Export()))return false;
        if(!files_.Available()){status_=IrStoreStatus::Unavailable;writable_=false;return false;}
        if(!SameStorage())return false;
        if(generation_==std::numeric_limits<uint32_t>::max()){status_=IrStoreStatus::GenerationExhausted;return false;}
        StateSnapshot<Payload> next{generation_+1,candidate};
        auto bytes=next.Encode();
        const unsigned bank=generation_ ? 1-active_ : 0;
        if(bytes.size()>Payload::MaxArchive+128 || !files_.WriteDurable(bank,bytes)){status_=IrStoreStatus::WriteFailed;writable_=false;return false;}
        std::string readback;StateSnapshot<Payload> verified;
        if(!files_.Read(bank,readback,Payload::MaxArchive+128) || readback!=bytes || !verified.Decode(readback)){status_=IrStoreStatus::WriteFailed;writable_=false;return false;}
        if(files_.Identity()!=identity_){status_=IrStoreStatus::StorageChanged;writable_=false;return false;}
        loaded_[bank]={true,bytes.size(),IrCrc32(bytes)};
        library_=candidate;generation_=next.generation;active_=bank;status_=IrStoreStatus::Ready;return true;
    }
public:
    explicit PersistentState(StateBankFiles& files):files_(files){}
    IrStoreStatus Status() const{return status_;}
    const Payload& Library() const{return library_;}
    bool Load() {
        writable_=false;
        if(!files_.Available()){status_=IrStoreStatus::Unavailable;writable_=false;return false;}
        identity_=files_.Identity();if(identity_.empty()){status_=IrStoreStatus::Unavailable;return false;}
        StateSnapshot<Payload> a,b;std::string first,second;
        const bool ae=files_.Exists(0),be=files_.Exists(1);
        const bool ar=!ae || files_.Read(0,first,Payload::MaxArchive+128);
        const bool br=!be || files_.Read(1,second,Payload::MaxArchive+128);
        if(!ar || !br){status_=IrStoreStatus::ReadFailed;return false;}
        loaded_[0]={ae,first.size(),IrCrc32(first)};
        loaded_[1]={be,second.size(),IrCrc32(second)};
        if(files_.Identity()!=identity_){status_=IrStoreStatus::StorageChanged;return false;}
        const bool av=ae && a.Decode(first);
        const bool bv=be && b.Decode(second);
        if(!ae && !be){library_.Clear();generation_=0;status_=IrStoreStatus::Empty;writable_=true;return true;}
        if(!av && !bv){status_=IrStoreStatus::Corrupt;return false;}
        if(av && bv && a.generation==b.generation && first!=second){status_=IrStoreStatus::Conflict;return false;}
        active_=bv && (!av || b.generation>a.generation) ? 1 : 0;
        const auto& selected=active_ ? b : a;
        library_=selected.library;generation_=selected.generation;writable_=true;
        status_=(ae&&!av)||(be&&!bv) ? IrStoreStatus::Recovered : IrStoreStatus::Ready;return true;
    }
    bool Save(const Payload& value){return Commit(value);}
};
}
