#pragma once
#include "toolbox_models.hpp"
#include <charconv>
#include <string_view>
namespace stock_tools {
// Original native implementations. No catalog/Furi code or remote database imported.
inline bool ValidSignal(const std::vector<uint32_t>& pulses)
{
    if (pulses.empty() || pulses.size()>511) return false;
    uint64_t total=0;
    for(auto pulse:pulses){if(!pulse || pulse>15000)return false;total+=pulse;}
    return total<=180000;
}
struct RemoteRecord { std::string name; std::vector<uint32_t> pulses; };
class RemoteLibrary {
    std::vector<RemoteRecord> records_;
    static bool Number(std::string_view text,uint32_t& value)
    {
        if(text.empty() || text.front()<'0' || text.front()>'9')return false;
        auto result=std::from_chars(text.data(),text.data()+text.size(),value);
        return result.ec==std::errc{} && result.ptr==text.data()+text.size();
    }
    static bool Name(std::string_view name)
    {
        if(name.empty() || name.size()>24 || name.front()==' ' || name.back()==' ')return false;
        for(auto c:name)if(!((c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') ||
                            c==' ' || c=='_' || c=='-' || c=='.'))return false;
        return true;
    }
public:
    static constexpr size_t MaxRecords=8,MaxArchive=65536;
    const std::vector<RemoteRecord>& Records() const {return records_;}
    bool Save(const std::string& name,const std::vector<uint32_t>& pulses)
    {
        if(records_.size()>=MaxRecords || !Name(name) || !ValidSignal(pulses))return false;
        for(const auto& record:records_)if(record.name==name)return false;
        records_.push_back({name,pulses});return true;
    }
    void Clear(){records_.clear();}
    // Versioned native archive, not a Flipper .ir/FAP import. Carrier remains fixed at38kHz.
    std::string Export() const
    {
        std::string result="STACKCHAN_IR_V1\n"+std::to_string(records_.size())+"\n";
        for(const auto& record:records_){
            result+=record.name+"\n38000\n"+std::to_string(record.pulses.size())+"\n";
            for(size_t i=0;i<record.pulses.size();++i){if(i)result+=' ';result+=std::to_string(record.pulses[i]);}
            result+='\n';
        }
        return result;
    }
    bool Import(std::string_view archive)
    {
        if(archive.size()>MaxArchive)return false;
        size_t cursor=0;
        auto line=[&](std::string_view& value){
            auto end=archive.find('\n',cursor);if(end==std::string_view::npos)return false;
            value=archive.substr(cursor,end-cursor);cursor=end+1;return true;
        };
        std::string_view value;
        uint32_t count=0;
        if(!line(value) || value!="STACKCHAN_IR_V1" || !line(value) || !Number(value,count) || count>MaxRecords)return false;
        RemoteLibrary candidate;
        for(uint32_t record=0;record<count;++record){
            if(!line(value) || !Name(value))return false;
            std::string name(value);
            uint32_t carrier=0,length=0;
            if(!line(value) || !Number(value,carrier) || carrier!=38000 || !line(value) ||
               !Number(value,length) || !length || length>511 || !line(value))return false;
            std::vector<uint32_t> pulses;
            for(uint32_t i=0;i<length;++i){
                auto end=value.find(' ');
                auto token=value.substr(0,end);
                uint32_t pulse=0;if(!Number(token,pulse))return false;
                pulses.push_back(pulse);
                if(i+1==length){if(end!=std::string_view::npos)return false;}
                else {if(end==std::string_view::npos)return false;value.remove_prefix(end+1);}
            }
            if(!candidate.Save(name,pulses))return false;
        }
        if(cursor!=archive.size())return false;
        records_=std::move(candidate.records_);return true;
    }
};
class RemoteStore {
public:
    virtual ~RemoteStore()=default;
    virtual bool Available() const=0;
    virtual bool Load(std::string& archive,size_t max_bytes)=0;
    virtual bool Save(std::string_view archive)=0;
};
class UnavailableRemoteStore final : public RemoteStore {
public:
    bool Available() const override{return false;}
    bool Load(std::string&,size_t) override{return false;}
    bool Save(std::string_view) override{return false;}
};
inline bool LoadRemotes(RemoteStore& store,RemoteLibrary& library)
{
    if(!store.Available())return false;
    std::string archive;
    return store.Load(archive,RemoteLibrary::MaxArchive) && library.Import(archive);
}
inline bool SaveRemotes(RemoteStore& store,const RemoteLibrary& library)
{
    if(!store.Available())return false;
    auto archive=library.Export();
    return archive.size()<=RemoteLibrary::MaxArchive && store.Save(archive);
}
enum class GrovePort { None,A,B,C };
struct UartConfig {GrovePort port=GrovePort::None;uint32_t baud=115200;};
inline bool ValidUartConfig(UartConfig config)
{
    return (config.port==GrovePort::A || config.port==GrovePort::B || config.port==GrovePort::C) &&
           (config.baud==9600 || config.baud==38400 || config.baud==115200);
}
// No GPIO/pin or UART6/7 mapping is activated. A future qualified Grove adapter
// must implement bounded nonblocking IO and truthful joined Close.
class UartTransport {
public:
    virtual ~UartTransport()=default;
    virtual bool Available() const=0;
    virtual bool Open(UartConfig)=0;
    virtual int Read(uint8_t*,size_t)=0;
    virtual bool Write(const uint8_t*,size_t)=0;
    virtual bool Close()=0;
};
class UnavailableUart final : public UartTransport {
public:
    bool Available() const override{return false;}
    bool Open(UartConfig) override{return false;}
    int Read(uint8_t*,size_t) override{return -1;}
    bool Write(const uint8_t*,size_t) override{return false;}
    bool Close() override{return true;}
};
// Explicit local echo demo for the dedicated page; never calls a hardware API.
class DemoUart : public UartTransport {
    std::string pending_;
    bool open_=false;
public:
    bool Available() const override{return true;}
    bool Open(UartConfig config) override
    {
        if(open_ || !ValidUartConfig(config))return false;
        open_=true;pending_="UART demo: no GPIO IO\n";return true;
    }
    int Read(uint8_t* bytes,size_t capacity) override
    {
        if(!open_)return -1;
        auto size=std::min(capacity,pending_.size());std::copy_n(pending_.begin(),size,bytes);
        pending_.erase(0,size);return static_cast<int>(size);
    }
    bool Write(const uint8_t* bytes,size_t count) override
    {
        if(!open_ || count>65 || pending_.size()+count+6>512)return false;
        pending_+="demo> ";pending_.append(reinterpret_cast<const char*>(bytes),count);return true;
    }
    bool Close() override{open_=false;pending_.clear();return true;}
};
enum class TerminalState { Ready,Active,Restoring,Closed,Unavailable,Error };
class TerminalSession {
    UartTransport& transport_;
    bool owned_=false;
    uint64_t started_=0;
    std::string text_;
    TerminalState state_=TerminalState::Ready;
    void Append(const uint8_t* bytes,size_t count)
    {
        static constexpr char hex[]="0123456789ABCDEF";
        for(size_t i=0;i<count;++i){
            auto c=bytes[i];
            if((c>=32 && c<=126) || c=='\n')text_+=static_cast<char>(c);
            else {text_+="\\x";text_+=hex[c>>4];text_+=hex[c&15];}
        }
        if(text_.size()>MaxText)text_.erase(0,text_.size()-MaxText);
    }
public:
    static constexpr size_t MaxText=1024;
    explicit TerminalSession(UartTransport& transport):transport_(transport){}
    bool Start(UartConfig config,uint64_t now)
    {
        if(owned_)return false;
        if(!ValidUartConfig(config)){state_=TerminalState::Error;return false;}
        if(!transport_.Available()){state_=TerminalState::Unavailable;return false;}
        owned_=true;
        if(!transport_.Open(config)){if(Stop())state_=TerminalState::Error;return false;}
        text_.clear();started_=now;state_=TerminalState::Active;return true;
    }
    bool Send(std::string_view line)
    {
        if(!owned_ || state_!=TerminalState::Active || line.empty() || line.size()>64)return false;
        for(auto c:line)if(c<32 || c>126)return false;
        std::string bytes(line);bytes+='\n';
        if(transport_.Write(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()))return true;
        if(Stop())state_=TerminalState::Error;
        return false;
    }
    void Poll(uint64_t now)
    {
        if(!owned_)return;
        if(WindowExpired(now,started_,60000)){Stop();return;}
        if(state_!=TerminalState::Active)return;
        std::array<uint8_t,64> bytes{};
        int count=transport_.Read(bytes.data(),bytes.size());
        if(count<0 || count>static_cast<int>(bytes.size())){if(Stop())state_=TerminalState::Error;return;}
        Append(bytes.data(),static_cast<size_t>(count));
    }
    bool Stop()
    {
        if(!owned_)return true;
        state_=TerminalState::Restoring;
        if(!transport_.Close())return false;
        owned_=false;state_=TerminalState::Closed;return true;
    }
    bool Clear(){if(!Stop())return false;text_.clear();return true;}
    const std::string& Text() const{return text_;}
    std::string Preview() const
    {
        size_t start=text_.size()>160?text_.size()-160:0;
        int lines=0;
        for(size_t i=text_.size();i>start;--i){
            if(text_[i-1]=='\n' && i<text_.size() && ++lines==3){start=i;break;}
        }
        return text_.substr(start);
    }
    TerminalState State() const{return state_;}
    bool Active() const{return owned_;}
};
} // namespace stock_tools
