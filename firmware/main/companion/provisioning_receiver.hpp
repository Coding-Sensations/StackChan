#pragma once
#include "provisioning_frame.hpp"
#include <array>
namespace companion {
enum class ProvisioningRead { Waiting, Rejected, Accepted };
// No I/O/persistence. Caller opens only after explicit physical approval of an
// independently qualified trusted provisioner. Qualification is never wire data.
class ProvisioningReceiver {
    std::array<uint8_t,755> bytes_{};
    size_t used_=0;
    uint64_t deadline_=0;
    std::string device_;
    ProvisioningWindow window_;
public:
    void Close() { std::fill(bytes_.begin(),bytes_.end(),0); used_=0; deadline_=0; device_.clear(); window_.Close(); }
    bool Open(uint64_t now,const std::string& device,bool qualified) {
        Close();
        if(!qualified || device.empty() || device.size()>100 || now>UINT64_MAX-60000) return false;
        device_=device; deadline_=now+60000; window_.Open(now); return true;
    }
    bool Active() const { return deadline_!=0; }
    ProvisioningRead Feed(const uint8_t* data,size_t n,uint64_t now,ProvisioningFields& out) {
        out.Clear();
        if(!Active() || now>=deadline_ || (n && !data) || n>bytes_.size()-used_) { Close(); return ProvisioningRead::Rejected; }
        if(n) { std::copy(data,data+n,bytes_.begin()+used_); used_+=n; }
        if(used_<7) return ProvisioningRead::Waiting;
        if(bytes_[0]!='S' || bytes_[1]!='C' || bytes_[2]!='P' || bytes_[3]!='G' || bytes_[4]!=1) { Close(); return ProvisioningRead::Rejected; }
        size_t target=7+(static_cast<size_t>(bytes_[5])<<8 | bytes_[6]);
        if(target<19 || target>bytes_.size() || used_>target) { Close(); return ProvisioningRead::Rejected; }
        if(used_<target) return ProvisioningRead::Waiting;
        bool ok=window_.Accept(bytes_.data(),used_,now,device_,true,out);
        Close(); return ok ? ProvisioningRead::Accepted : ProvisioningRead::Rejected;
    }
};
}
