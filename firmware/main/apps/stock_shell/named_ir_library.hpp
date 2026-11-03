#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace stock_tools {
// Native raw captures start with a mark and alternate mark/space, in microseconds.
struct NamedIrCommand {
    std::string device, command;
    uint32_t carrier_hz = 38000;
    std::vector<uint32_t> pulses;
};
class NamedIrLibrary {
    std::vector<NamedIrCommand> records_;
    static bool Number(std::string_view text, uint32_t& value) {
        if (text.empty() || text.front()<'0' || text.front()>'9') return false;
        auto result=std::from_chars(text.data(),text.data()+text.size(),value);
        return result.ec==std::errc{} && result.ptr==text.data()+text.size();
    }
public:
    static constexpr size_t MaxRecords=64, MaxPulses=511, MaxArchive=262144;
    static bool ValidName(std::string_view name) {
        if(name.empty() || name.size()>24 || name.front()==' ' || name.back()==' ')return false;
        for(char c:name) if(!((c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') || c==' ' || c=='_' || c=='-' || c=='.'))return false;
        return true;
    }
    static bool Valid(const NamedIrCommand& value) {
        if(!ValidName(value.device) || !ValidName(value.command) || value.carrier_hz<30000 || value.carrier_hz>60000 || value.pulses.empty() || value.pulses.size()>MaxPulses)return false;
        uint64_t total=0;
        for(auto pulse:value.pulses){if(!pulse || pulse>15000)return false;total+=pulse;}
        return total<=180000;
    }
    const std::vector<NamedIrCommand>& Records() const {return records_;}
    const NamedIrCommand* Find(std::string_view device,std::string_view command) const {
        for(const auto& r:records_)if(r.device==device && r.command==command)return &r;
        return nullptr;
    }
    // Explicit replacement flag prevents accidental overwrite of an existing button.
    bool Save(const NamedIrCommand& command,bool replace=false) {
        if(!Valid(command))return false;
        for(auto& r:records_)if(r.device==command.device && r.command==command.command){if(!replace)return false;r=command;return true;}
        if(records_.size()>=MaxRecords)return false;
        records_.push_back(command);return true;
    }
    bool Remove(std::string_view device,std::string_view command) {
        auto found=std::find_if(records_.begin(),records_.end(),[&](const auto& r){return r.device==device && r.command==command;});
        if(found==records_.end())return false;
        records_.erase(found);return true;
    }
    void Clear(){records_.clear();}
    std::string Export() const {
        std::string text="STACKCHAN_NAMED_IR_V2\n"+std::to_string(records_.size())+"\n";
        for(const auto& r:records_){
            text+=r.device+"\n"+r.command+"\n"+std::to_string(r.carrier_hz)+"\n"+std::to_string(r.pulses.size())+"\n";
            for(size_t i=0;i<r.pulses.size();++i){if(i)text+=' ';text+=std::to_string(r.pulses[i]);}
            text+='\n';
        }
        return text;
    }
    bool Import(std::string_view archive) {
        if(archive.size()>MaxArchive)return false;
        size_t cursor=0;
        auto line=[&](std::string_view& value){auto end=archive.find('\n',cursor);if(end==std::string_view::npos)return false;value=archive.substr(cursor,end-cursor);cursor=end+1;return true;};
        std::string_view value;
        uint32_t count=0;
        if(!line(value) || value!="STACKCHAN_NAMED_IR_V2" || !line(value) || !Number(value,count) || count>MaxRecords)return false;
        NamedIrLibrary candidate;
        for(uint32_t n=0;n<count;++n){
            NamedIrCommand r;
            if(!line(value) || !ValidName(value))return false;
            r.device=value;
            if(!line(value) || !ValidName(value))return false;
            r.command=value;
            uint32_t length=0;
            if(!line(value) || !Number(value,r.carrier_hz) || !line(value) || !Number(value,length) || !length || length>MaxPulses || !line(value))return false;
            for(uint32_t i=0;i<length;++i){
                auto end=value.find(' ');
                uint32_t pulse=0;
                if(!Number(value.substr(0,end),pulse))return false;
                r.pulses.push_back(pulse);
                if(i+1==length){if(end!=std::string_view::npos)return false;}
                else{if(end==std::string_view::npos)return false;value.remove_prefix(end+1);}
            }
            if(!candidate.Save(r))return false;
        }
        if(cursor!=archive.size())return false;
        records_=std::move(candidate.records_);return true;
    }
};
inline uint32_t IrCrc32(std::string_view bytes) {
    uint32_t crc=0xffffffff;
    for(unsigned char byte:bytes){crc^=byte;for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320U & (0U-(crc&1)));}
    return ~crc;
}
// Each bank is independently checksum protected. A torn inactive-bank write
// never requires deleting/renaming the last good bank (including on FAT).
struct IrSnapshot {
    uint32_t generation=0;
    NamedIrLibrary library;
    std::string Encode() const {
        const auto payload=library.Export();
        auto body=std::to_string(generation)+"\n"+std::to_string(payload.size())+"\n"+payload;
        return "STACKCHAN_IR_BANK_V1\n"+std::to_string(IrCrc32(body))+"\n"+body;
    }
    bool Decode(std::string_view bytes) {
        if(bytes.size()>NamedIrLibrary::MaxArchive+128)return false;
        size_t cursor=0;
        auto line=[&](std::string_view& value){auto end=bytes.find('\n',cursor);if(end==std::string_view::npos)return false;value=bytes.substr(cursor,end-cursor);cursor=end+1;return true;};
        auto number=[](std::string_view v,uint32_t& n){if(v.empty() || v.front()<'0' || v.front()>'9')return false;auto r=std::from_chars(v.data(),v.data()+v.size(),n);return r.ec==std::errc{} && r.ptr==v.data()+v.size();};
        std::string_view value;uint32_t checksum=0,gen=0,length=0;
        if(!line(value) || value!="STACKCHAN_IR_BANK_V1" || !line(value) || !number(value,checksum))return false;
        const auto body=bytes.substr(cursor);
        if(checksum!=IrCrc32(body) || !line(value) || !number(value,gen) || !gen || !line(value) || !number(value,length) || length>NamedIrLibrary::MaxArchive || bytes.size()-cursor!=length)return false;
        NamedIrLibrary candidate;
        if(!candidate.Import(bytes.substr(cursor)))return false;
        generation=gen;library=std::move(candidate);return true;
    }
};
class IrBankFiles {
public:
    virtual ~IrBankFiles()=default;
    virtual bool Available() const=0;
    virtual bool Exists(unsigned bank) const=0;
    virtual bool Read(unsigned bank,std::string& data,size_t maximum)=0;
    // Return success only after write, flush, sync and close all succeeded.
    virtual bool WriteDurable(unsigned bank,std::string_view data)=0;
};
enum class IrStoreStatus { Unavailable, Empty, Ready, Recovered, Corrupt, Conflict, ReadFailed, StorageChanged, WriteFailed, GenerationExhausted };
class PersistentIrLibrary {
    IrBankFiles& files_;
    NamedIrLibrary library_;
    uint32_t generation_=0;
    unsigned active_=0;
    IrStoreStatus status_=IrStoreStatus::Unavailable;
    bool writable_=false;
    struct Fingerprint {bool exists=false;size_t size=0;uint32_t crc=0;};
    std::array<Fingerprint,2> loaded_{};
    bool SameStorage() {
        for(unsigned bank=0;bank<2;++bank){
            const bool exists=files_.Exists(bank);
            if(exists!=loaded_[bank].exists){status_=IrStoreStatus::StorageChanged;writable_=false;return false;}
            if(!exists)continue;
            std::string bytes;
            if(!files_.Read(bank,bytes,NamedIrLibrary::MaxArchive+128)){status_=IrStoreStatus::ReadFailed;writable_=false;return false;}
            if(bytes.size()!=loaded_[bank].size || IrCrc32(bytes)!=loaded_[bank].crc){status_=IrStoreStatus::StorageChanged;writable_=false;return false;}
        }
        return true;
    }
    bool Commit(const NamedIrLibrary& candidate) {
        if(!writable_)return false;
        if(!files_.Available()){status_=IrStoreStatus::Unavailable;return false;}
        if(!SameStorage())return false;
        if(generation_==std::numeric_limits<uint32_t>::max()){status_=IrStoreStatus::GenerationExhausted;return false;}
        IrSnapshot next{generation_+1,candidate};
        auto bytes=next.Encode();
        const unsigned bank=generation_ ? 1-active_ : 0;
        if(bytes.size()>NamedIrLibrary::MaxArchive+128 || !files_.WriteDurable(bank,bytes)){status_=IrStoreStatus::WriteFailed;return false;}
        std::string readback;IrSnapshot verified;
        if(!files_.Read(bank,readback,NamedIrLibrary::MaxArchive+128) || readback!=bytes || !verified.Decode(readback)){status_=IrStoreStatus::WriteFailed;return false;}
        loaded_[bank]={true,bytes.size(),IrCrc32(bytes)};
        library_=candidate;generation_=next.generation;active_=bank;status_=IrStoreStatus::Ready;return true;
    }
public:
    explicit PersistentIrLibrary(IrBankFiles& files):files_(files){}
    IrStoreStatus Status() const{return status_;}
    const NamedIrLibrary& Library() const{return library_;}
    bool Load() {
        writable_=false;
        if(!files_.Available()){status_=IrStoreStatus::Unavailable;return false;}
        IrSnapshot a,b;std::string first,second;
        const bool ae=files_.Exists(0),be=files_.Exists(1);
        const bool ar=!ae || files_.Read(0,first,NamedIrLibrary::MaxArchive+128);
        const bool br=!be || files_.Read(1,second,NamedIrLibrary::MaxArchive+128);
        if(!ar || !br){status_=IrStoreStatus::ReadFailed;return false;}
        loaded_[0]={ae,first.size(),IrCrc32(first)};
        loaded_[1]={be,second.size(),IrCrc32(second)};
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
    bool Save(const NamedIrCommand& command,bool replace=false) {auto next=library_;return next.Save(command,replace) && Commit(next);}
    bool Remove(std::string_view device,std::string_view command) {auto next=library_;return next.Remove(device,command) && Commit(next);}
};
} // namespace stock_tools
