#pragma once
#include <algorithm>
#include <optional>
#include <vector>

namespace launcher_focus {
struct Selection { int index; bool immediate; };
// Identity belongs to this list. A pixel offset from another list is never input.
inline Selection Select(const std::vector<int>& ids,std::optional<int> preferred,int warm_index) {
    if(ids.empty())return {0,false};
    if(preferred){
        const auto found=std::find(ids.begin(),ids.end(),*preferred);
        return {found==ids.end()?0:static_cast<int>(found-ids.begin()),true};
    }
    // Keep legacy explicit warm positioning for callers without semantic navigation.
    if(warm_index>=0)return {std::min(warm_index,static_cast<int>(ids.size())-1),true};
    return {0,false};
}
}
