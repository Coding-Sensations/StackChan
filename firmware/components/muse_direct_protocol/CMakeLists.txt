# Compose with the owner-resolved numeric build profile before IDF registration.
if(NOT DEFINED STACKCHAN_PROFILE_MUSE OR NOT DEFINED STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL)
    include("${CMAKE_CURRENT_LIST_DIR}/../../build_profiles.cmake")
endif()
if(STACKCHAN_PROFILE_MUSE STREQUAL "0")
    idf_component_register()
    return()
endif()

if(NOT STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL MATCHES "^[01]$")
    message(FATAL_ERROR "Resolved numeric Muse voice protocol selection required")
endif()
set(muse_voice_protocol_sources "")
if(STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL STREQUAL "1")
    list(APPEND muse_voice_protocol_sources "voice/api.cc" "voice/muse_voice_call_core.c" "voice/service_exchange.cc" "voice/exchange_api.cc")
endif()

# Isolated protocol-only port. No SDK app, board, BLE, NVS, Wi-Fi, OTA or UI.
idf_component_register(
 SRCS "direct_pairing_api.c" "pairing/link_pairing.c" "pairing/pairing_signer_policy.c" "pairing/pairing_transcript.c"
      "noise_core/src/ClientSession.cpp" "noise_core/src/InitiatorHandshake.cpp"
      "noise_core/src/MbedtlsCryptoBackend.cpp" "noise_core/src/ServiceCodec.cpp"
      "noise_core/src/Status.cpp" "noise_core/src/Transport.cpp"
      "noise_core/src/TransportFrameCodec.cpp" ${muse_voice_protocol_sources}
 INCLUDE_DIRS "." "pairing" "noise_core/include"
 PRIV_REQUIRES mbedtls espressif__cjson esp_timer esp_system efuse
 WHOLE_ARCHIVE)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# Link a dormant function table, not an init hook; nothing invokes it at boot.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,stackchan_muse_direct_pairing")

if(STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL STREQUAL "1")
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,stackchan_muse_voice_protocol_api" "-Wl,-u,stackchan_muse_voice_exchange_api")
endif()
