#pragma once
#include "named_ir_library.hpp"
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace stock_tools {
// The board adapter must supply a real mounted-card predicate and an existing
// private directory on that volume. This adapter does not guess SD pins or mount
// a volume. A host directory in tests is not evidence of on-device SD support.
class MountedIrBankFiles final : public IrBankFiles {
    std::string directory_;
    bool (*mounted_)() = nullptr;
    std::string Path(unsigned bank) const {return directory_+(bank ? "/IR_B.DAT" : "/IR_A.DAT");}
public:
    MountedIrBankFiles(std::string directory,bool (*mounted)()):directory_(std::move(directory)),mounted_(mounted){}
    bool Available() const override {
        struct stat info{};
        return mounted_ && mounted_() && !directory_.empty() && directory_.front()=='/' &&
               stat(directory_.c_str(),&info)==0 && S_ISDIR(info.st_mode);
    }
    bool Exists(unsigned bank) const override {
        if(bank>1 || !Available())return true; // Error is not an empty library.
        struct stat info{};
        if(stat(Path(bank).c_str(),&info)==0)return true;
        return errno!=ENOENT;
    }
    bool Read(unsigned bank,std::string& output,size_t maximum) override {
        if(bank>1 || !Available() || maximum>NamedIrLibrary::MaxArchive+128)return false;
        FILE* file=fopen(Path(bank).c_str(),"rb");
        if(!file)return false;
        std::string candidate;char block[512];bool ok=true;
        for(;;){
            auto count=fread(block,1,sizeof(block),file);
            if(candidate.size()+count>maximum){ok=false;break;}
            candidate.append(block,count);
            if(count<sizeof(block)){ok=!ferror(file);break;}
        }
        if(fclose(file)!=0)ok=false;
        if(!ok)return false;
        output=std::move(candidate);return true;
    }
    bool WriteDurable(unsigned bank,std::string_view bytes) override {
        if(bank>1 || !Available() || bytes.size()>NamedIrLibrary::MaxArchive+128)return false;
        FILE* file=fopen(Path(bank).c_str(),"wb");
        if(!file)return false;
        bool ok=fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();
        if(ok)ok=fflush(file)==0;
        if(ok)ok=fsync(fileno(file))==0;
        if(fclose(file)!=0)ok=false;
        return ok;
    }
};
} // namespace stock_tools
