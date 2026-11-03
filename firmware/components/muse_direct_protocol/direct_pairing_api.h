#pragma once
// Dormant protocol API only. The trusted caller must own an explicitly
// approved Muse pairing window, exclusive BLE lease and stable volatile
// identity/token storage before using any entry. No automatic IO or storage.
#ifdef __cplusplus
extern "C" {
#endif
#include "link_pairing.h"
typedef struct {
    uint32_t version;
    const char* physical_policy;
    void (*init)(const char*,const char*,const char*,const char*,const char*);
    void (*reset)(void);
    // After joining all work, drop borrowed identity/token references before
    // their owner wipes/releases its buffers. reset alone keeps those references.
    void (*shutdown)(void);
    void (*device_info)(cJSON*);
    const char* (*hello)(cJSON*,char**);
    const char* (*decrypt)(cJSON*,char**);
    uint32_t (*finished)(void);
    bool (*arm_press)(uint32_t);
    uint32_t (*physical_press)(void);
    uint32_t (*provisioning)(void);
    bool (*commit)(uint32_t,bool(*)(void));
    char* (*encrypted_status)(const char*,uint32_t,uint32_t*);
    char* (*encrypted_json)(const char*,uint32_t,uint32_t*);
} stackchan_muse_direct_pairing_api_t;
const stackchan_muse_direct_pairing_api_t* stackchan_muse_direct_pairing(void);
#ifdef __cplusplus
}
#endif
