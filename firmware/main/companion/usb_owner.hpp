#pragma once
#include "provisioning_receiver.hpp"
#include "provisioning_ack.hpp"
#include <array>
namespace companion {
enum class UsbOwnershipError { None, Disabled, WrongCore, ForeignDriver, InstallFailed, InvalidWindow, StaleInput, ReadFailed, Expired, DriverLost, CloseFailed, AcknowledgementFailed };
// Driver is the SDK boundary only. One serialized caller on CPU0 owns all
// receiver state and the installed interrupt. A foreign driver is never touched.
template<class Driver> class UsbProvisioningOwner {
    Driver& driver_;
    ProvisioningReceiver receiver_;
    bool owned_=false,awaiting_stage_=false;
    std::string accepted_request_;
    uint64_t deadline_=0;
    UsbOwnershipError error_=UsbOwnershipError::None;
    static void Wipe(std::array<uint8_t,128>& bytes) {
        volatile uint8_t* p=bytes.data();
        for(size_t i=0;i<bytes.size();++i)p[i]=0;
    }
    bool Emit(const AckFrame& frame) {
        if(!owned_||!driver_.Installed()||!driver_.OnOwnerCore())return false;
        // One write and one bounded SDK flush; never retry a partial receipt.
        int n=driver_.WriteAck(frame.data(),frame.size());
        bool ok=n==static_cast<int>(frame.size())&&driver_.FlushAck();
        if(!ok)error_=UsbOwnershipError::AcknowledgementFailed;
        return ok;
    }
public:
    explicit UsbProvisioningOwner(Driver& driver):driver_(driver){}
    ~UsbProvisioningOwner(){Close();}
    UsbProvisioningOwner(const UsbProvisioningOwner&)=delete;
    UsbProvisioningOwner& operator=(const UsbProvisioningOwner&)=delete;
    bool OwnsDriver()const{return owned_;}
    bool Active()const{return owned_&&receiver_.Active();}
    UsbOwnershipError LastError()const{return error_;}
    bool Close(){
        receiver_.Close();deadline_=0;awaiting_stage_=false;accepted_request_.clear();
        if(!owned_)return true;
        if(!driver_.Installed()){owned_=false;error_=UsbOwnershipError::DriverLost;return true;}
        if(!driver_.OnOwnerCore()){error_=UsbOwnershipError::WrongCore;return false;}
        if(!driver_.Uninstall()){error_=UsbOwnershipError::CloseFailed;return false;}
        owned_=false;return true;
    }
    bool Open(uint64_t now,const std::string& device,bool qualified,uint32_t boot_nonce=1,uint32_t reset_reason=0){
        if(!Close())return false;
        error_=UsbOwnershipError::None;
        if(!qualified){error_=UsbOwnershipError::Disabled;return false;}
        if(!boot_nonce||reset_reason>64){error_=UsbOwnershipError::InvalidWindow;return false;}
        if(!driver_.OnOwnerCore()){error_=UsbOwnershipError::WrongCore;return false;}
        if(driver_.Installed()){error_=UsbOwnershipError::ForeignDriver;return false;}
        if(!receiver_.Open(now,device,true)){error_=UsbOwnershipError::InvalidWindow;return false;}
        if(!driver_.Install()){receiver_.Close();error_=UsbOwnershipError::InstallFailed;return false;}
        owned_=true;deadline_=now+60000;
        std::array<uint8_t,128> stale{};bool found=false;
        // Never turn a pre-consent FIFO frame into a grant. Drain at most the
        // configured 1024-byte RX capacity, reject any stale bytes, then close.
        for(unsigned i=0;i<8;++i){
            int n=driver_.Read(stale.data(),stale.size());Wipe(stale);
            if(n<0||n>static_cast<int>(stale.size())){error_=UsbOwnershipError::ReadFailed;Close();return false;}
            if(!n)break;
            found=true;
        }
        if(found){error_=UsbOwnershipError::StaleInput;Emit(EncodeStatusAck(AckStatus::Rejected));Close();return false;}
        if(!boot_nonce||reset_reason>64||!Emit(EncodeReadyAck(boot_nonce,reset_reason))){Close();return false;}
        return true;
    }
    ProvisioningRead Poll(uint64_t now,ProvisioningFields& fields){
        fields.Clear();
        if(!Active())return ProvisioningRead::Rejected;
        if(!driver_.Installed()){Close();return ProvisioningRead::Rejected;}
        if(!driver_.OnOwnerCore()){error_=UsbOwnershipError::WrongCore;Close();return ProvisioningRead::Rejected;}
        // Expiry is checked before driver reads, not after consuming a token.
        if(now>=deadline_){error_=UsbOwnershipError::Expired;Emit(EncodeStatusAck(AckStatus::Expired));Close();return ProvisioningRead::Rejected;}
        std::array<uint8_t,128> bytes{};
        int n=driver_.Read(bytes.data(),bytes.size());
        if(n<0||n>static_cast<int>(bytes.size())){Wipe(bytes);error_=UsbOwnershipError::ReadFailed;Close();return ProvisioningRead::Rejected;}
        auto result=receiver_.Feed(bytes.data(),static_cast<size_t>(n),now,fields);Wipe(bytes);
        if(result==ProvisioningRead::Accepted){
            // Receiver window is already closed. Retain the SDK owner until
            // the caller has staged the grant and sent a truthful receipt.
            awaiting_stage_=true;accepted_request_=fields.request;return result;
        }
        if(result==ProvisioningRead::Rejected){Emit(EncodeStatusAck(AckStatus::Rejected));Close();}
        return result;
    }
    bool CompleteAccepted(bool staged,const std::string& request){
        if(!owned_||!awaiting_stage_||request!=accepted_request_||!driver_.Installed()||!driver_.OnOwnerCore())return false;
        awaiting_stage_=false;
        bool sent=Emit(staged?EncodeStagedAck(request):EncodeStatusAck(AckStatus::Rejected));
        // A delivered Staged receipt remains truthful even if teardown fails.
        // Ownership is retained on failure; Connect must retry/require Close.
        Close();return sent;
    }
};
}
