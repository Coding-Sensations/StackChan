#pragma once
#include <array>
namespace view::saver {
// Persisted IDs remain unchanged: DVD=0, upstream catalog index+1.
// Remove whole light/flashing and red/blue full-background modes, including ChessModels.
// Moire (ID226) is removed after a reported device freeze; Moire2 stays ID225.
// Hydrostat (157) and Metaballs (220) are physically pruned at build time.
// Michael's 74 additional source-verified, spelling-confirmed hidden effects are pruned.
// Stable IDs remain tombstones; intentional neutral gray and colored foregrounds remain.
constexpr std::array<unsigned,202> ExcludedSlides={3,4,5,6,7,8,9,11,12,13,14,15,16,17,18,19,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,39,40,41,43,44,45,46,47,48,49,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,68,70,71,74,76,80,81,88,90,91,92,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,112,113,114,115,116,118,119,121,122,124,125,126,127,130,133,134,135,136,138,139,142,144,146,147,148,150,151,156,157,175,179,181,192,195,197,199,201,203,204,206,208,209,210,213,214,217,220,221,226,229,231,233,237,238,239,240,241,242,243,244,245,246,250,251,252,256,257,260,261,262,264,269,271,274,275,279,280,282,284,285,288,289,290,293,294,298,301,302,303,304,306,307,308,309,310,311,312,313,314,316,317,318,319,322,323,324,326,327,328,330,331,334,335,338,341};
constexpr bool Allowed(unsigned id) {
    if(id>=342)return false;
    for(auto excluded:ExcludedSlides)if(id==excluded)return false;
    return true;
}
constexpr unsigned AllowedCount=342-ExcludedSlides.size();
inline unsigned AllowedOrdinal(unsigned id){unsigned n=0;for(unsigned i=0;i<=id&&i<342;++i)n+=Allowed(i);return n;}
inline unsigned AdjacentAllowed(unsigned id,bool forward){for(unsigned n=0;n<342;++n){id=(id+(forward?1:341))%342;if(Allowed(id))return id;}return 0;}
}
