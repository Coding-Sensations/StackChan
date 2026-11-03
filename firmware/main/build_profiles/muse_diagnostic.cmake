# Final source assembly only; this diagnostic introduces no new component edge.
option(STACKCHAN_MUSE_LOCAL_DIAGNOSTIC "Explicit local cold retirement diagnostic startup (no microphone/PCM/network)" OFF)
set(STACKCHAN_DIAGNOSTIC_ENABLED 0)
if(STACKCHAN_MUSE_LOCAL_DIAGNOSTIC)
    set(STACKCHAN_DIAGNOSTIC_ENABLED 1)
    if(NOT CMAKE_BUILD_EARLY_EXPANSION)
        if(NOT CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN OR
           NOT STACKCHAN_PROFILE_MUSE STREQUAL "1" OR
           NOT STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL STREQUAL "1" OR
           NOT STACKCHAN_MUSE_RAM_ONLY)
            message(FATAL_ERROR "Cold diagnostic requires CoreS3 StackChan, Muse+voice and explicit RAM-only configuration")
        endif()
        if(STACKCHAN_COMPANION_REMEMBERED_RUNTIME OR STACKCHAN_COMPANION_WIRELESS_PAIRING OR
           STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING OR STACKCHAN_ENABLE_LOCAL_PORTRAITS OR
           STACKCHAN_LOCAL_PORTRAIT_MANIFEST OR STACKCHAN_COMPANION_BINDING_CONFIG OR STACKCHAN_COMPANION_LOCAL_CA_FILE OR STACKCHAN_COMPANION_LOCAL_TLS_HOST)
            message(FATAL_ERROR "Cold diagnostic forbids saved/live routes, provisioning, network trust and local artwork")
        endif()
    endif()
else()
    list(FILTER SOURCES EXCLUDE REGEX "(^|/)companion/muse_local_diagnostic[.]cc$")
endif()
