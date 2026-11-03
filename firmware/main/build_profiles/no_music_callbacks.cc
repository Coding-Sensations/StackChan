#include <usb_composite_v2/startup.h>
// Keep Mira's shared composite transport, with no local synth/MIDI callbacks.
// Startup and all existing physical qualification/cleanup gates remain intact.
namespace usb_composite_v2 {
Integration BindProductCallbacks(Integration binding) {
    binding.local_note = nullptr;
    binding.local_silence = nullptr;
    binding.local_arm = nullptr;
    return binding;
}
}
