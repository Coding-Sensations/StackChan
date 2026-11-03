#pragma once
// An explicitly reviewed private build may provide this ignored header.
#if __has_include("personal_binding.local.hpp")
#include "personal_binding.local.hpp"
#endif
#ifndef STACKCHAN_PIN_DEVICE
#define STACKCHAN_PIN_DEVICE ""
#endif
#ifndef STACKCHAN_PIN_HOST
#define STACKCHAN_PIN_HOST ""
#endif
#ifndef STACKCHAN_PIN_URI
#define STACKCHAN_PIN_URI ""
#endif
#ifndef STACKCHAN_PIN_CA_SHA256
#define STACKCHAN_PIN_CA_SHA256 ""
#endif
#ifndef STACKCHAN_PIN_EXPIRY
#define STACKCHAN_PIN_EXPIRY 0U
#endif
namespace companion {
inline bool PersonalBindingAvailable() {
 return STACKCHAN_PIN_DEVICE[0] && STACKCHAN_PIN_HOST[0] && STACKCHAN_PIN_URI[0] &&
        sizeof(STACKCHAN_PIN_CA_SHA256)==65 && STACKCHAN_PIN_EXPIRY>0;
}
}

#define STACKCHAN_PIN_PAIR_URI "wss://" STACKCHAN_PIN_HOST ":18443/companion/pair"
#define STACKCHAN_PIN_FEATURE_URI "wss://" STACKCHAN_PIN_HOST ":18443/companion/features"
