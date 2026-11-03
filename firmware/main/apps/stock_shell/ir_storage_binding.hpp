#pragma once
#include "mounted_ir_bank_files.hpp"
#include "stackchan_sd_ir.h"
#include <memory>
namespace stock_tools {
// Configure once during board initialization, after the existing board-owned
// SD adapter has mounted the card and created its private directory. No new bus
// or pin is owned here. Until then every operation reports unavailable.
inline std::unique_ptr<IrBankFiles>& IrMountedFiles() {
    static std::unique_ptr<IrBankFiles> files;
    return files;
}
inline bool ConfigureIrBankFiles(std::unique_ptr<IrBankFiles> files) {
    if(IrMountedFiles() || !files || !files->Available())return false;
    IrMountedFiles()=std::move(files);return true;
}
inline bool ConfigureIrMountedFiles(std::string directory,bool (*mounted)()) {
    if(IrMountedFiles() || !mounted)return false;
    auto files=std::make_unique<MountedIrBankFiles>(std::move(directory),mounted);
    if(!files->Available())return false;
    IrMountedFiles()=std::move(files);return true;
}
class ConfiguredIrBankFiles final : public IrBankFiles {
public:
    bool Available() const override{return IrMountedFiles() && IrMountedFiles()->Available();}
    bool Exists(unsigned bank) const override{return IrMountedFiles() ? IrMountedFiles()->Exists(bank) : true;}
    bool Read(unsigned bank,std::string& data,size_t maximum) override{return IrMountedFiles() && IrMountedFiles()->Read(bank,data,maximum);}
    bool WriteDurable(unsigned bank,std::string_view bytes) override{return IrMountedFiles() && IrMountedFiles()->WriteDurable(bank,bytes);}
};
inline const char* IrStoreMessage(IrStoreStatus status) {
    switch(status){
        case IrStoreStatus::Unavailable:return StackChanSdUnavailableText();
        case IrStoreStatus::Empty:return "SD library empty";
        case IrStoreStatus::Ready:return "Saved on SD";
        case IrStoreStatus::Recovered:return "Recovered previous save";
        case IrStoreStatus::Corrupt:return "SD library damaged";
        case IrStoreStatus::Conflict:return "SD library conflict";
        case IrStoreStatus::ReadFailed:return "SD read failed; reload";
        case IrStoreStatus::StorageChanged:return "SD changed; reload";
        case IrStoreStatus::WriteFailed:return "SD save failed";
        case IrStoreStatus::GenerationExhausted:return "SD generation exhausted";
    }
    return "SD error";
}
} // namespace stock_tools
