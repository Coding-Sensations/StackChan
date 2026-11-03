#pragma once
#include "usb_serial_byte_owner.h"
#include "usb_authenticated_endpoint.h"
#include "usb_control_dispatcher.h"
#include "usb_pair_messages.h"
namespace direct_usb_pcm {
enum class SetupIssue {None,ClockNotReady,TrustUnavailable,Ownership,Protocol};
enum class RuntimeState {Unselected,Provisioning,Tls,Challenge,Confirmed,WaitingReady,Media,Closing,Closed,Fault};
class Runtime {
 SerialByteOwner bytes_;companion::UsbTlsClient tls_;companion::NumericPairingContext pair_;
 UsbConsentIssuer issuer_;AuthenticatedEndpoint endpoint_;Worker worker_;
 std::unique_ptr<ControlDispatcher> dispatcher_;RecordParser parser_;
 std::array<uint8_t,4125> input_{};
 std::array<uint8_t,22> ready_{};size_t ready_sent_=0;std::array<uint8_t,26> request_bytes_{};size_t request_used_=0;uint32_t boot_=0;
 std::function<uint64_t()> now_;std::function<AudioCodec*()> codec_;
 uint32_t epoch_=0;uint16_t stream_=0;uint64_t cutoff_=0;
 SetupIssue issue_=SetupIssue::None;
 RuntimeState state_=RuntimeState::Unselected;bool staged_=false,failed_=false;
 bool Message(uint8_t,const uint8_t*,size_t);
 void Fail();
public:
 Runtime(std::function<uint64_t()>,std::function<AudioCodec*()>);
 // Trusted selected controller supplies immutable public request/boot/epoch /
 // non-reused stream/cutoff. No media authority is granted by this selection.
 bool PrepareSelected(uint32_t boot,uint32_t epoch,uint16_t stream,uint64_t cutoff);
 bool StartSelected(const std::string& request,uint32_t boot,uint32_t epoch,uint16_t stream,uint64_t cutoff);
 std::string ConsentStamp()const;
 bool ConfirmDisplayedCode(const std::string& code,const std::string& stamp);void Stop();void Tick();
 SetupIssue Issue()const{return issue_;}
 RuntimeState State()const{return state_;}const std::string& Code()const{return issuer_.DisplayedCode();}
 Snapshot Audio(){return worker_.Status();}
 bool Closed()const{return worker_.Closed()&&tls_.State()==companion::UsbTlsState::Closed&&!bytes_.HasCustody();}
};
}
