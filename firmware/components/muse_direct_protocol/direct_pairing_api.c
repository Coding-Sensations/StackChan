#include "direct_pairing_api.h"
#include "pairing_transcript.h"
// Constant table: no constructor, initialization call, NVS, radio, capture,
// driver ownership or token lookup. Retaining it in the image keeps the actual
// pairing core available for the next reviewed device lifecycle integration.
static void shutdown_protocol(void){link_pairing_init(NULL,NULL,NULL,NULL,NULL);}
static const stackchan_muse_direct_pairing_api_t api={
 PAIRING_VERSION,PAIRING_POLICY_BUTTON,link_pairing_init,link_pairing_reset,shutdown_protocol,
 link_pairing_add_device_info,link_pairing_handle_client_hello,link_pairing_decrypt_command,
 link_pairing_handle_client_finished,link_pairing_arm_confirmation,
 link_pairing_confirm_active_session,link_pairing_mark_provisioning_active,
 link_pairing_commit_provisioning,link_pairing_encrypt_status,link_pairing_encrypt_json
};
const stackchan_muse_direct_pairing_api_t* stackchan_muse_direct_pairing(void){return &api;}
