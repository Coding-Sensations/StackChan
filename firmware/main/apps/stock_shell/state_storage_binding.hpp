#pragma once
#include "saved_tools.hpp"
#include <memory>
namespace stock_tools {
// Factory uses the already mounted board SD owner, same LCD drain/pin lease.
std::unique_ptr<StateBankFiles> MakeStackChanStateFiles(const char* stem,size_t maximum);
inline std::unique_ptr<StateBankFiles>& PetFiles(){static auto f=MakeStackChanStateFiles("PET",SavedPet::MaxArchive+128);return f;}
inline std::unique_ptr<StateBankFiles>& WakeFiles(){static auto f=MakeStackChanStateFiles("WOL",SavedWakeTargets::MaxArchive+128);return f;}
}
