#pragma once
#include <usb_composite_v2/cdc_session.hpp>
#include <optional>
namespace companion {
// Selected boot transport, not a permission decision. Requested composite never
// falls back to Serial/JTAG; partial-close CDC custody stays in this object.
template<class Legacy,class Bridge,class Mode> class FeatureUsbDriver {
 Legacy legacy_;Mode mode_;std::optional<usb_composite_v2::CdcSessionDriver<Bridge>> cdc_;
 auto* Cdc(){auto* bridge=mode_.BridgeNow();if(!bridge)return static_cast<usb_composite_v2::CdcSessionDriver<Bridge>*>(nullptr);if(!cdc_)cdc_.emplace(*bridge);return &*cdc_;}
public:
 bool OnOwnerCore()const{return legacy_.OnOwnerCore();}
 bool Installed(){if(!mode_.Requested())return legacy_.Installed();auto* c=Cdc();return c&&c->Installed();}
 bool Install(){if(!mode_.Requested())return legacy_.Install();auto* c=Cdc();return c&&c->Install();}
 bool Uninstall(){if(!mode_.Requested())return legacy_.Uninstall();return !cdc_||cdc_->Uninstall();}
 int Read(uint8_t* p,size_t n){if(!mode_.Requested())return legacy_.Read(p,n);auto* c=Cdc();return c?c->Read(p,n):-1;}
 int WriteAck(const uint8_t* p,size_t n){if(!mode_.Requested())return legacy_.WriteAck(p,n);auto* c=Cdc();return c?c->WriteAck(p,n):-1;}
 bool FlushAck(){if(!mode_.Requested())return legacy_.FlushAck();auto* c=Cdc();return c&&c->FlushAck();}
};
}
