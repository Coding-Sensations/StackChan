// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_session.hpp"
namespace companion::muse {
class DeviceCredentialReader {
public:
 virtual ~DeviceCredentialReader()=default;
 virtual bool LoadAccess(const std::string& exact_node,std::function<bool()> current,DeviceRecord& output)=0;
};
struct VmCredential {
 std::string vm;Secret<3072> bearer;
 void Clear(){vm.clear();bearer.Clear();}
};
class ExactVmLookup {
public:
 virtual ~ExactVmLookup()=default;
 virtual bool FindExactVm(const DeviceRecord&,const std::string& exact_vm,VmCredential& output)=0;
};
inline bool SelectExactVm(cJSON* root,const std::string& vm,VmCredential& output){
 output.Clear();if(!Id(vm)||!Unique(root)||cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"error"))||!Text(root,"error_title").empty())return false;
 auto* list=cJSON_GetObjectItemCaseSensitive(root,"vm_list");if(!cJSON_IsArray(list))return false;const cJSON* selected=nullptr;
 for(auto* item=list->child;item;item=item->next){if(!Unique(item))return false;if(Text(item,"vm_id")!=vm)continue;if(selected)return false;selected=item;}
 auto* token=cJSON_GetObjectItemCaseSensitive(selected,"vm_auth_token");if(!selected||!cJSON_IsString(token)||!token->valuestring||!output.bearer.Set(token->valuestring))return false;
 output.vm=vm;return true;
}
}
