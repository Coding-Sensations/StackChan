#pragma once
#include "persistent_state.hpp"
#include <apps/local_tools/local_tools.hpp>
#include <apps/network_sensor/wol_runtime.hpp>
namespace stock_tools {
inline bool StateNumber(std::string_view v,uint64_t& out){if(v.empty()||v.front()<'0'||v.front()>'9')return false;auto r=std::from_chars(v.data(),v.data()+v.size(),out);return r.ec==std::errc{}&&r.ptr==v.data()+v.size();}
inline bool StateLine(std::string_view& bytes,std::string_view& line){auto e=bytes.find('\n');if(e==std::string_view::npos)return false;line=bytes.substr(0,e);bytes.remove_prefix(e+1);return true;}
struct SavedPet {
 static constexpr size_t MaxArchive=128;
 stackchan_tools::Pet pet;
 void Clear(){pet={};}
 bool Valid()const{return pet.hunger<=100&&pet.happiness<=100&&pet.energy<=100&&pet.remainder_ms<60000&&pet.decay_phase_minutes<6;}
 std::string Export()const{return "STACKCHAN_PET_V1\n"+std::to_string(pet.hunger)+"\n"+std::to_string(pet.happiness)+"\n"+std::to_string(pet.energy)+"\n"+std::to_string(pet.remainder_ms)+"\n"+std::to_string(pet.decay_phase_minutes)+"\n";}
 bool Import(std::string_view bytes){if(bytes.size()>MaxArchive)return false;std::string_view line;if(!StateLine(bytes,line)||line!="STACKCHAN_PET_V1")return false;uint64_t v[5]{};for(auto& n:v)if(!StateLine(bytes,line)||!StateNumber(line,n))return false;if(!bytes.empty()||v[0]>100||v[1]>100||v[2]>100||v[3]>=60000||v[4]>=6)return false;pet={uint8_t(v[0]),uint8_t(v[1]),uint8_t(v[2]),v[3],uint8_t(v[4])};return true;}
};
struct WakeTarget{std::string name,mac,broadcast;};
class SavedWakeTargets {
 std::vector<WakeTarget> values_;
public:
 static constexpr size_t MaxArchive=1024,MaxRecords=8;
 const std::vector<WakeTarget>& Records()const{return values_;}
 void Clear(){values_.clear();}
 static bool Valid(const WakeTarget& target){stack_tools::MacAddress mac{};uint32_t ip=0;stack_tools::MagicPacket packet{};return NamedIrLibrary::ValidName(target.name)&&stack_tools::ParseMac(target.mac,mac)&&stack_tools::MakeMagicPacket(mac,packet)&&stack_tools::ParseIpv4(target.broadcast,ip);}
 bool Save(const WakeTarget& target,bool replace=false){if(!Valid(target))return false;for(auto& v:values_)if(v.name==target.name){if(!replace)return false;v=target;return true;}if(values_.size()>=MaxRecords)return false;values_.push_back(target);return true;}
 bool Remove(std::string_view name){auto it=std::find_if(values_.begin(),values_.end(),[&](const auto& t){return t.name==name;});if(it==values_.end())return false;values_.erase(it);return true;}
 std::string Export()const{std::string out="STACKCHAN_WOL_V1\n"+std::to_string(values_.size())+"\n";for(auto& t:values_)out+=t.name+"\n"+t.mac+"\n"+t.broadcast+"\n";return out;}
 bool Import(std::string_view bytes){if(bytes.size()>MaxArchive)return false;std::string_view line;uint64_t count=0;if(!StateLine(bytes,line)||line!="STACKCHAN_WOL_V1"||!StateLine(bytes,line)||!StateNumber(line,count)||count>MaxRecords)return false;SavedWakeTargets next;for(uint64_t i=0;i<count;++i){WakeTarget target;if(!StateLine(bytes,line))return false;target.name=line;if(!StateLine(bytes,line))return false;target.mac=line;if(!StateLine(bytes,line))return false;target.broadcast=line;if(!next.Save(target))return false;}if(!bytes.empty())return false;values_=std::move(next.values_);return true;}
};
}
