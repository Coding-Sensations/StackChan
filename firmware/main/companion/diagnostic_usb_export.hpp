#pragma once
#include <cstddef>
#include <cstdint>
namespace companion {
inline constexpr std::size_t DiagnosticUsbMaxBytes=256;
enum class DiagnosticExportResult { NotAttempted,NotStopped,WrongCore,InvalidFrame,CleanupFailed,ForeignDriver,InstallFailed,WriteFailed,FlushFailed,Exported };
inline const char* DiagnosticExportMessage(DiagnosticExportResult result) {
    switch(result) {
    case DiagnosticExportResult::NotStopped:return "Export needs fully stopped audio and USB";
    case DiagnosticExportResult::WrongCore:return "Export unavailable on this task";
    case DiagnosticExportResult::InvalidFrame:return "Diagnostic snapshot invalid";
    case DiagnosticExportResult::CleanupFailed:return "USB cleanup pending - stay muted";
    case DiagnosticExportResult::ForeignDriver:return "USB busy - export not started";
    case DiagnosticExportResult::InstallFailed:return "USB export could not start";
    case DiagnosticExportResult::WriteFailed:return "USB write failed - export unconfirmed";
    case DiagnosticExportResult::FlushFailed:return "USB flush unconfirmed - check host capture";
    case DiagnosticExportResult::Exported:return "Diagnostics exported - no audio";
    default:return "Diagnostic export not requested";
    }
}
// Explicit diagnostics only: no receive, Ready, Pair or audio admission.
// One serialized CPU0 caller owns its SDK interrupt. Foreign drivers are untouched.
template<class Driver> class DiagnosticUsbExport {
    Driver& driver_; bool owned_=false;
    DiagnosticExportResult result_=DiagnosticExportResult::NotAttempted;
    bool Failed(DiagnosticExportResult result){result_=result;return false;}
public:
    explicit DiagnosticUsbExport(Driver& driver):driver_(driver){}
    bool Close(){
        if(!owned_)return true;
        if(!driver_.Installed()){owned_=false;return true;}
        if(!driver_.OnOwnerCore()||!driver_.Uninstall())return false;
        owned_=false;return true;
    }
    DiagnosticExportResult LastResult()const{return result_;}
    bool Export(bool fully_stopped,const uint8_t* frame,std::size_t bytes){
        if(!fully_stopped)return Failed(DiagnosticExportResult::NotStopped);
        if(!frame||!bytes||bytes>DiagnosticUsbMaxBytes)return Failed(DiagnosticExportResult::InvalidFrame);
        if(!driver_.OnOwnerCore())return Failed(DiagnosticExportResult::WrongCore);
        if(!Close())return Failed(DiagnosticExportResult::CleanupFailed);
        if(driver_.Installed())return Failed(DiagnosticExportResult::ForeignDriver);
        if(!driver_.InstallDiagnostic())return Failed(DiagnosticExportResult::InstallFailed);
        owned_=true;
        // One bounded SDK write/flush; no partial-write retry.
        result_=DiagnosticExportResult::Exported;
        if(driver_.WriteAck(frame,bytes)!=static_cast<int>(bytes))result_=DiagnosticExportResult::WriteFailed;
        else if(!driver_.FlushAck())result_=DiagnosticExportResult::FlushFailed;
        if(!Close())return Failed(DiagnosticExportResult::CleanupFailed);
        return result_==DiagnosticExportResult::Exported;
    }
    bool OwnsDriver()const{return owned_;}
};
}
