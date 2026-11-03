#include "host_client.hpp"
#include <usb/usb_types_stack.h>
#include <cstring>
namespace stackchan_midi_host {
void HostClient::Push(const Event& e){
 if(stopping_.load())return;
 if(e.kind==Kind::Invalid){++rejected_;return;}if(e.kind==Kind::Ignored||e.kind==Kind::PitchBend)return;
 const unsigned head=head_.load(std::memory_order_relaxed),next=(head+1)%events_.size();
 if(next==tail_.load(std::memory_order_acquire)){panic_=true;++rejected_;return;}events_[head]=e;
 if(stopping_.load())return;
 head_.store(next,std::memory_order_release);
}
void HostClient::ClientEvent(const usb_host_client_event_msg_t* msg,void* arg){
 auto& self=*static_cast<HostClient*>(arg);
 if(msg->event==USB_HOST_CLIENT_EVENT_NEW_DEV&&!self.stopping_&&!self.device_&&!self.pending_address_)self.pending_address_=msg->new_dev.address;
 else if(msg->event==USB_HOST_CLIENT_EVENT_DEV_GONE&&msg->dev_gone.dev_hdl==self.device_){self.gone_=true;self.panic_=true;}
}
void HostClient::Transfer(usb_transfer_t* transfer){
 auto& self=*static_cast<HostClient*>(transfer->context);self.inflight_=false;
 if(transfer->status==USB_TRANSFER_STATUS_COMPLETED&&!self.stopping_&&!self.gone_){
  if(transfer->actual_num_bytes<0||transfer->actual_num_bytes>transfer->num_bytes||transfer->actual_num_bytes%4){self.panic_=true;++self.rejected_;}
  else for(int i=0;i<transfer->actual_num_bytes;i+=4){
#ifdef MIDI_HOST_TESTING
   extern void HostPacketFixtureHook(int);HostPacketFixtureHook(i);
#endif
   if(self.stopping_.load())break;
   self.Push(Packet(transfer->data_buffer+i,4,self.interface_.cables));
  }
  self.submit_=true;
 }else {self.gone_=true;self.panic_=true;}
}
bool HostClient::RegisterAfterHostInstalled(){
 if(client_||device_||transfer_)return false;
 usb_host_lib_info_t info{};if(usb_host_lib_info(&info)!=ESP_OK)return false;
 usb_host_client_config_t cfg{};cfg.is_synchronous=false;cfg.max_num_event_msg=4;cfg.async.client_event_callback=ClientEvent;cfg.async.callback_arg=this;
 if(usb_host_client_register(&cfg,&client_)!=ESP_OK)return false;
 closed_=false;gone_=false;pending_address_=0;return true;
}
bool HostClient::Open(uint8_t address){
 if(usb_host_device_open(client_,address,&device_)!=ESP_OK)return false;
 const usb_config_desc_t* config=nullptr;
 if(usb_host_get_active_config_descriptor(device_,&config)!=ESP_OK||!config||!Descriptor(reinterpret_cast<const uint8_t*>(config),config->wTotalLength,interface_))return false;
 if(usb_host_interface_claim(client_,device_,interface_.number,0)!=ESP_OK)return false;
 claimed_=true;
 if(usb_host_transfer_alloc(interface_.packet_size,0,&transfer_)!=ESP_OK)return false;
 transfer_->device_handle=device_;transfer_->bEndpointAddress=interface_.endpoint;transfer_->callback=Transfer;transfer_->context=this;transfer_->num_bytes=interface_.packet_size;submit_=true;return true;
}
bool HostClient::Close(){
 submit_=false;
 if(inflight_){
  // Halt/flush only our claimed IN endpoint. Completion is still owned by the
  // event task; retain transfer/device/client until callback confirms it ended.
  const auto halt=usb_host_endpoint_halt(device_,interface_.endpoint);
  if(halt!=ESP_OK)return false;
  if(usb_host_endpoint_flush(device_,interface_.endpoint)!=ESP_OK)return false;
  return false;
 }
 if(transfer_){if(usb_host_transfer_free(transfer_)!=ESP_OK)return false;transfer_=nullptr;}
 if(claimed_){if(usb_host_interface_release(client_,device_,interface_.number)!=ESP_OK)return false;
 claimed_=false;}
 if(device_){if(usb_host_device_close(client_,device_)!=ESP_OK)return false;device_=nullptr;}
 return true;
}
bool HostClient::Poll(){
 if(!client_)return Closed();
 const auto events=usb_host_client_handle_events(client_,0);if(events!=ESP_OK&&events!=ESP_ERR_TIMEOUT){stopping_=true;panic_=true;}
 if(stopping_||gone_){if(!Close())return false;gone_=false;pending_address_=0;if(stopping_){if(usb_host_client_deregister(client_)!=ESP_OK)return false;client_=nullptr;closed_=true;return true;}}
 if(pending_address_&&!device_){const auto address=pending_address_;pending_address_=0;if(!Open(address)){gone_=true;panic_=true;return false;}}
 if(submit_&&!inflight_&&!stopping_&&!gone_){submit_=false;if(usb_host_transfer_submit(transfer_)==ESP_OK)inflight_=true;else{gone_=true;panic_=true;}}
 return false;
}
}
