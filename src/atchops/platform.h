#ifndef ATCHOPS_PLATFORM_H
#define ATCHOPS_PLATFORM_H

// NOTE: there are two platform specific files:
// - atchops/platform.h
// - atclient/json.h

// Arduino/ESP32 NoPorts embedded build (skipped when building native unit tests)
#ifndef ATCHOPS_TARGET_NATIVE
#define ATCHOPS_TARGET_ARDUINO
#define ATCLIENT_SOCKET_PROVIDER_EXTERNAL
// Auto-detect mbedTLS major version from the ESP-IDF header.
// Classic ESP32/S2/S3 ship with ESP-IDF 4.x (mbedTLS 2.x).
// ESP32-P4 and newer targets use ESP-IDF 5.x (mbedTLS 3.x).
#include <mbedtls/version.h>
#if MBEDTLS_VERSION_MAJOR < 3
#define ATCHOPS_MBEDTLS_VERSION_2
#endif
#else
// Native build: auto-detect mbedTLS version so tests work on both
// Ubuntu (libmbedtls-dev ≈ v2.x) and macOS (brew ≈ v3/v4).
#include <mbedtls/version.h>
#if MBEDTLS_VERSION_MAJOR < 3
#define ATCHOPS_MBEDTLS_VERSION_2
#endif
#endif

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif

// Fallback platform detection (not used on Arduino, but kept for reference)
#if !defined(ATCHOPS_TARGET_ARDUINO)
  #if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define ATCHOPS_TARGET_UNIX
  #elif defined(_WIN32)
    #define ATCHOPS_TARGET_WINDOWS
  #elif defined(CONFIG_IDF_TARGET_ESP32)
    #define ATCHOPS_TARGET_ESPIDF
  #endif
#endif

#endif // ATCHOPS_PLATFORM_H
