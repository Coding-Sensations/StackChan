#pragma once
#include <cstdint>
#include <string>
#include "screensaver_filter.hpp"
#include "effect_catalog/effect_catalog.hpp"
namespace view::saver {
// ID0 remains the original DVD; UI IDs1..341 map to source catalog indexes.
// Preserve upstream ordering when persisting selections.
enum class Style : uint16_t { Dvd = 0 };
inline unsigned CatalogCount() noexcept { return 1u + effect_catalog::Count(); }
inline Style FromStored(int value) noexcept {
    return value >= 0 && unsigned(value) < CatalogCount() && Allowed(unsigned(value)) ? static_cast<Style>(value) : Style::Dvd;
}
inline const char* Name(Style style) noexcept {
    const auto id=static_cast<unsigned>(style);
    return id==0 || id>=CatalogCount() ? "DVD" : effect_catalog::Name(uint16_t(id-1));
}
inline const char* CatalogOptions() {
    static const std::string options=[] {
        std::string names="DVD";
        for(uint16_t i=0;i<effect_catalog::Count();++i) {
            if(!Allowed(unsigned(i)+1))continue;
            names+='\n'; names+=effect_catalog::Name(i);
        }
        return names;
    }();
    return options.c_str();
}
} // namespace view::saver
