#include "persistent_pairing.hpp"
#include <nvs.h>
#include <mbedtls/sha256.h>
#include <ctime>
extern "C" const char* companion_local_ca_pem();
extern "C" uint32_t companion_local_pair_expiry_unix();
namespace companion {
namespace {
// Each operation owns only sc_pair. Never erase the partition or another namespace.
class NvsPairStorage {
 nvs_handle_t handle_=0;bool opened_=false;
public:
 explicit NvsPairStorage(bool writable){opened_=nvs_open("sc_pair",writable?NVS_READWRITE:NVS_READONLY,&handle_)==ESP_OK;}
 ~NvsPairStorage(){if(opened_)nvs_close(handle_);}
 bool Empty(){size_t size=0;return opened_&&nvs_get_blob(handle_,"record",nullptr,&size)==ESP_ERR_NVS_NOT_FOUND;}
 bool Read(PairRecord& r){size_t size=0;if(!opened_||nvs_get_blob(handle_,"record",nullptr,&size)!=ESP_OK||size!=sizeof r)return false;return nvs_get_blob(handle_,"record",&r,&size)==ESP_OK&&size==sizeof r;}
 bool Set(const PairRecord& r){return opened_&&nvs_set_blob(handle_,"record",&r,sizeof r)==ESP_OK;}
 bool Commit(){return opened_&&nvs_commit(handle_)==ESP_OK;}
 bool Erase(){return opened_&&nvs_erase_all(handle_)==ESP_OK;}
};
}
PairPolicy CurrentPairPolicy(){
 PairPolicy p;p.device=PairDevice;p.uri=PairUri;p.expiry=companion_local_pair_expiry_unix();
 const auto now=std::time(nullptr);if(now>0&&uint64_t(now)<=UINT32_MAX)p.now=uint32_t(now);
 const char* pem=companion_local_ca_pem();unsigned char hash[32]={};
 if(pem&&*pem&&mbedtls_sha256(reinterpret_cast<const unsigned char*>(pem),std::strlen(pem),hash,0)==0){static const char hex[]="0123456789abcdef";for(auto byte:hash){p.ca.push_back(hex[byte>>4]);p.ca.push_back(hex[byte&15]);}}
 return p;
}
bool RememberedPairStoreEmpty(){NvsPairStorage nvs(true);return nvs.Empty();}
bool LoadRememberedPair(PairRecord& r){NvsPairStorage nvs(false);PairStore<NvsPairStorage> store(nvs);return store.Load(r,CurrentPairPolicy());}
bool SaveRememberedPair(const PairRecord& r){NvsPairStorage nvs(true);PairStore<NvsPairStorage> store(nvs);return store.Save(r,CurrentPairPolicy());}
bool ForgetRememberedPair(){NvsPairStorage nvs(true);PairStore<NvsPairStorage> store(nvs);return store.Forget();}
}
