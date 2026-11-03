#include "personal_binding.hpp"
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
namespace companion {
inline constexpr const char* PairDevice=STACKCHAN_PIN_DEVICE;
inline constexpr const char* PairUri=STACKCHAN_PIN_URI;
// Fixed bounded blob; zero initialized including reserved bytes. Never log/format this type.
struct PairRecord {uint32_t version=0,expiry=0;char device[64]={},uri[128]={},ca[65]={},credential[257]={};};
inline void ClearPairRecord(PairRecord& r){volatile unsigned char* p=reinterpret_cast<volatile unsigned char*>(&r);for(size_t i=0;i<sizeof r;++i)p[i]=0;}
struct PairPolicy {std::string device,uri,ca;uint32_t expiry=0,now=0;};
inline bool PairHash(const std::string& s){if(s.size()!=64)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
inline bool PairSecret(const std::string& s){if(s.size()<32||s.size()>256)return false;for(unsigned char c:s)if(c<33||c>126)return false;return true;}
inline bool ValidPairRecord(const PairRecord& r,const PairPolicy& p){
 if(!PersonalBindingAvailable()||p.device!=PairDevice||p.uri!=PairUri||p.ca!=STACKCHAN_PIN_CA_SHA256||p.expiry!=STACKCHAN_PIN_EXPIRY||r.version!=1||!p.now||!p.expiry||p.now>=p.expiry||r.expiry!=p.expiry||!PairHash(p.ca))return false;
 if(!memchr(r.device,0,sizeof r.device)||!memchr(r.uri,0,sizeof r.uri)||!memchr(r.ca,0,sizeof r.ca)||!memchr(r.credential,0,sizeof r.credential))return false;
 return p.device==r.device&&p.uri==r.uri&&p.ca==r.ca&&PairSecret(r.credential);
}
inline bool MakePairRecord(PairRecord& r,const PairPolicy& p,const std::string& secret){
 ClearPairRecord(r);if(p.device.size()>=sizeof r.device||p.uri.size()>=sizeof r.uri||!PairSecret(secret)||!PairHash(p.ca))return false;
 r.version=1;r.expiry=p.expiry;memcpy(r.device,p.device.c_str(),p.device.size()+1);memcpy(r.uri,p.uri.c_str(),p.uri.size()+1);memcpy(r.ca,p.ca.c_str(),p.ca.size()+1);memcpy(r.credential,secret.c_str(),secret.size()+1);
 if(ValidPairRecord(r,p))return true;
 ClearPairRecord(r);return false;
}
template<class Storage> class PairStore {
 Storage& storage_;
public:
 explicit PairStore(Storage& s):storage_(s){}
 bool Load(PairRecord& out,const PairPolicy& p){ClearPairRecord(out);if(storage_.Read(out)&&ValidPairRecord(out,p))return true;ClearPairRecord(out);return false;}
 bool Save(const PairRecord& record,const PairPolicy& p){
  if(!ValidPairRecord(record,p)||!storage_.Empty()||!storage_.Set(record)||!storage_.Commit())return false;
  PairRecord readback;bool ok=storage_.Read(readback)&&ValidPairRecord(readback,p)&&memcmp(&record,&readback,sizeof record)==0;ClearPairRecord(readback);return ok;
 }
 bool Forget(){return storage_.Erase()&&storage_.Commit()&&storage_.Empty();}
};
// SDK boundary used by boot only; tests use PairStore with in-memory storage.
PairPolicy CurrentPairPolicy();bool RememberedPairStoreEmpty();bool LoadRememberedPair(PairRecord&);bool SaveRememberedPair(const PairRecord&);bool ForgetRememberedPair();
}
