/**
 * @file atsdk_socket.h
 * @brief Arduino/ESP32 socket provider for the atSDK
 *
 * This file provides the struct definitions for atclient_tls_socket and
 * atclient_raw_socket using ESP32's WiFiClientSecure. It is included by
 * atclient/socket_shared.h when ATCLIENT_SOCKET_PROVIDER_EXTERNAL is defined.
 */

#ifndef ATSDK_SOCKET_H
#define ATSDK_SOCKET_H

#include <stddef.h> // size_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The TLS socket struct wraps an opaque pointer to a WiFiClientSecure
 * object allocated in the C++ implementation (atsdk_socket.cpp).
 */
struct atclient_tls_socket {
  void *wifi_client;      // WiFiClientSecure* (opaque from C)
  int read_timeout_ms;    // Read timeout in milliseconds
  int configured;         // Whether CA certs have been loaded
  int connected;          // Connection state
  void *read_buf;         // Persistent read buffer (READ_BUF_SIZE), allocated once
  size_t read_buf_len;    // Bytes held in read_buf from an earlier read: the start of a
                          // line it timed out inside of, or what followed the line it returned
};

/**
 * Raw (non-TLS) socket — wraps an opaque pointer to WiFiClient.
 * Not typically used in the atSDK on Arduino.
 */
struct atclient_raw_socket {
  void *wifi_client;      // WiFiClient* (opaque from C)
  int connected;          // Connection state
};

#ifdef __cplusplus
}
#endif

#endif // ATSDK_SOCKET_H
