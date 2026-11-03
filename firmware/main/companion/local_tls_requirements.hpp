#pragma once
#include <mbedtls/build_info.h>

// Wall-clock availability alone does not enable X.509 validity-period checks.
// Check the library's effective configuration, rather than a local define.
#if !defined(MBEDTLS_HAVE_TIME) || !defined(MBEDTLS_HAVE_TIME_DATE)
#error "Companion TLS requires certificate date validation in mbedTLS (CONFIG_MBEDTLS_HAVE_TIME_DATE=y)"
#endif
