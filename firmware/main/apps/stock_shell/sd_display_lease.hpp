#pragma once
namespace stock_tools {
class SdDisplayOps {
public:
    virtual ~SdDisplayOps()=default;
    virtual bool Healthy() const=0;
    virtual bool Lock()=0;
    virtual bool DrainLcd()=0;
    virtual bool Input()=0;
    virtual bool Restore()=0;
    virtual void Unlock()=0;
    virtual void Fault()=0;
};
// GPIO35 is shared by LCD DC and SD MISO. No filesystem operation may run
// before LCD transactions are drained, or after direction restoration fails.
class SdDisplayLease {
    SdDisplayOps& ops_;
    bool locked_=false,restore_=false,closed_=false,ok_=true;
public:
    explicit SdDisplayLease(SdDisplayOps& ops):ops_(ops){}
    bool Open() {
        if(closed_ || locked_ || !ops_.Healthy())return false;
        if(!ops_.Lock()){closed_=true;ok_=false;return false;}
        locked_=true;
        if(!ops_.DrainLcd()){ok_=false;Close();return false;}
        restore_=true; // Even a partially failed pin transition needs restoration.
        if(!ops_.Input()){ok_=false;Close();return false;}
        return true;
    }
    bool Close() {
        if(closed_)return ok_;
        closed_=true;
        if(restore_ && !ops_.Restore()){
            ok_=false;ops_.Fault();
            // Preserve exclusion of LCD transfers if GPIO35 could still be
            // input. A physical restart is required after this terminal fault.
            return false;
        }
        if(locked_)ops_.Unlock();
        return ok_;
    }
    ~SdDisplayLease(){Close();}
};
} // namespace stock_tools
