#pragma once
#include "instrument_engine.hpp"
#include <apps/stock_shell/persistent_state.hpp>
#include <apps/stock_shell/state_storage_binding.hpp>
namespace stock_tools {
struct NamedInstrumentProject{std::string name;stackchan_instruments::Project project;};
class InstrumentProjects {
 std::vector<NamedInstrumentProject> records_;
 static int Hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
 static bool Line(std::string_view& bytes,std::string_view& value){auto end=bytes.find('\n');if(end==std::string_view::npos)return false;value=bytes.substr(0,end);bytes.remove_prefix(end+1);return true;}
public:
 static constexpr size_t MaxArchive=1024,MaxRecords=8;
 void Clear(){records_.clear();}
 const std::vector<NamedInstrumentProject>& Records()const{return records_;}
 const stackchan_instruments::Project* Find(std::string_view name)const{for(const auto& record:records_)if(record.name==name)return &record.project;return nullptr;}
 bool Save(std::string_view name,const stackchan_instruments::Project& project,bool replace=false){
  if(!NamedIrLibrary::ValidName(name)||!stackchan_instruments::ValidProject(project))return false;
  for(auto& record:records_)if(record.name==name){if(!replace)return false;record.project=project;return true;}
  if(records_.size()>=MaxRecords){return false;}
  records_.push_back({std::string(name),project});return true;
 }
 bool Remove(std::string_view name){auto it=std::find_if(records_.begin(),records_.end(),[&](const auto& record){return record.name==name;});if(it==records_.end())return false;records_.erase(it);return true;}
 std::string Export()const{
  static constexpr const char* hex="0123456789abcdef";std::string output="STACKCHAN_MUSIC_V1\n"+std::to_string(records_.size())+"\n";
  for(const auto& record:records_){output+=record.name+"\n";for(auto b:stackchan_instruments::EncodeProject(record.project)){output+=hex[b>>4];output+=hex[b&15];}output+='\n';}return output;
 }
 bool Import(std::string_view bytes){
  if(bytes.size()>MaxArchive){return false;}
  std::string_view value;uint32_t count=0;
  if(!Line(bytes,value)||value!="STACKCHAN_MUSIC_V1"||!Line(bytes,value)||value.empty()||value.front()<'0'||value.front()>'9')return false;
  auto parsed=std::from_chars(value.data(),value.data()+value.size(),count);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||count>MaxRecords)return false;
  InstrumentProjects next;
  for(uint32_t i=0;i<count;++i){
   if(!Line(bytes,value)||!NamedIrLibrary::ValidName(value)){return false;}
   std::string name(value);
   if(!Line(bytes,value)||value.size()!=64){return false;}
   std::array<uint8_t,32> encoded{};
   for(size_t n=0;n<32;++n){int hi=Hex(value[n*2]),lo=Hex(value[n*2+1]);if(hi<0||lo<0)return false;encoded[n]=uint8_t((hi<<4)|lo);}
   stackchan_instruments::Project project;if(!stackchan_instruments::DecodeProject(encoded.data(),encoded.size(),project)||!next.Save(name,project))return false;
  }
  if(!bytes.empty()){return false;}
  records_=std::move(next.records_);return true;
 }
};
// Synchronous storage operations: stop/join the audio owner first; never call
// Load/Save from Render or a realtime/audio callback. No automatic retry.
class InstrumentProjectStore {
 PersistentState<InstrumentProjects> state_;
public:
 explicit InstrumentProjectStore(StateBankFiles& files):state_(files){}
 bool Load(){return state_.Load();}
 IrStoreStatus Status()const{return state_.Status();}
 const InstrumentProjects& Library()const{return state_.Library();}
 bool Save(std::string_view name,const stackchan_instruments::Project& project,bool replace=false){auto next=state_.Library();return next.Save(name,project,replace)&&state_.Save(next);}
 bool Remove(std::string_view name){auto next=state_.Library();return next.Remove(name)&&state_.Save(next);}
};
inline StateBankFiles& InstrumentProjectFiles(){static auto files=MakeStackChanStateFiles("MUS",InstrumentProjects::MaxArchive+128);return *files;}
}
