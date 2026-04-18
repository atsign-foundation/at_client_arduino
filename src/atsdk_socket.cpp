/**
 * @file atsdk_socket.cpp
 * @brief Arduino/ESP32 socket implementation for the atSDK
 *
 * Implements atclient_tls_socket_* functions using WiFiClientSecure.
 * This is the "external socket provider" that replaces the mbedTLS-based
 * socket implementation used on Linux/macOS.
 */

#include "atsdk_socket.h"
#include "atclient/socket.h"
#include "atclient/cacerts.h"
#include "atlogger/atlogger.h"

// Concatenate the bundled CA certificates into a single PEM string.
// Define ATSDK_TLS_MINIMAL_CERTS on memory-constrained devices (e.g. ESP32 with
// display) to use only the Let's Encrypt root, saving ~10KB heap.
#ifdef ATSDK_TLS_MINIMAL_CERTS
static const char atclient_cacerts_pem[] =
  LETS_ENCRYPT_ROOT;
#else
static const char atclient_cacerts_pem[] =
  LETS_ENCRYPT_ROOT
  GOOGLE_GLOBAL_SIGN
  GOOGLE_GTS_ROOT_R1
  GOOGLE_GTS_ROOT_R2
  GOOGLE_GTS_ROOT_R3
  GOOGLE_GTS_ROOT_R4
  ZEROSSL_INTERMEDIATE;
#endif

#include <WiFiClientSecure.h>
#include <cstring>
#include <cstdlib>

#define TAG "atsdk_socket"

// Default read timeout (5 seconds)
#define DEFAULT_READ_TIMEOUT_MS 5000

// Max read buffer size
#define READ_BUF_SIZE (4 * 1024)

// ============================================================
// TLS Socket
// ============================================================

extern "C" {

void atclient_tls_socket_init(struct atclient_tls_socket *socket) {
  if (socket == NULL) return;
  socket->wifi_client = nullptr;
  socket->read_timeout_ms = DEFAULT_READ_TIMEOUT_MS;
  socket->configured = 0;
  socket->connected = 0;
  socket->read_buf = NULL;
}

int atclient_tls_socket_configure(struct atclient_tls_socket *socket,
                                  unsigned char *ca_pem, size_t ca_pem_len) {
  if (socket == NULL) return 1;

  WiFiClientSecure *client = new WiFiClientSecure();
  if (client == nullptr) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to allocate WiFiClientSecure\n");
    return 1;
  }

  // Certificate verification setup.
  // On memory-constrained devices (ESP32 with display/DMA), PEM cert parsing
  // fragments the heap and causes mbedtls_ssl_setup() to fail.
  // Define ATSDK_USE_CERT_BUNDLE to use the ESP-IDF built-in cert bundle
  // which verifies from flash without heap allocation.
#ifdef ATSDK_USE_CERT_BUNDLE
  extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
  client->setCACertBundle(x509_crt_bundle_start);
#else
  // Use provided CA or the built-in atclient PEM certs
  if (ca_pem != NULL && ca_pem_len > 0) {
    client->setCACert((const char *)ca_pem);
  } else {
    client->setCACert((const char *)atclient_cacerts_pem);
  }
#endif

  client->setTimeout(socket->read_timeout_ms / 1000);

  // Pre-allocate the read buffer once so atclient_tls_socket_read never
  // needs to malloc on every call.  Per-read malloc was the root cause of
  // spurious monitor/worker reconnects: when relay pbufs fragment the heap
  // the 4 KB malloc failed, the SDK returned a read error, and the daemon
  // tore down a perfectly healthy TLS connection to reconnect.
  if (socket->read_buf == NULL) {
    socket->read_buf = malloc(READ_BUF_SIZE);
    if (socket->read_buf == NULL) {
      atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to pre-allocate read buffer\n");
      delete client;
      return 1;
    }
  }

  socket->wifi_client = client;
  socket->configured = 1;
  return 0;
}

void atclient_tls_socket_free(struct atclient_tls_socket *socket) {
  if (socket == NULL) return;
  if (socket->read_buf != NULL) {
    free(socket->read_buf);
    socket->read_buf = NULL;
  }
  if (socket->wifi_client != nullptr) {
    WiFiClientSecure *client = (WiFiClientSecure *)socket->wifi_client;
    if (client->connected()) {
      client->stop();
    }
    delete client;
    socket->wifi_client = nullptr;
  }
  socket->configured = 0;
  socket->connected = 0;
}

int atclient_tls_socket_connect(struct atclient_tls_socket *socket,
                                const char *host, const uint16_t port) {
  if (socket == NULL || host == NULL) return 1;

  if (!socket->configured || socket->wifi_client == nullptr) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                 "Socket not configured, call atclient_tls_socket_configure first\n");
    return 1;
  }

  WiFiClientSecure *client = (WiFiClientSecure *)socket->wifi_client;

  atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_DEBUG,
               "Connecting to %s:%u\n", host, port);

  if (!client->connect(host, port)) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                 "TLS connect to %s:%u failed\n", host, port);
    return 1;
  }

  socket->connected = 1;
  atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_DEBUG,
               "Connected to %s:%u\n", host, port);
  return 0;
}

int atclient_tls_socket_disconnect(struct atclient_tls_socket *socket) {
  if (socket == NULL) return 1;
  if (socket->wifi_client != nullptr) {
    WiFiClientSecure *client = (WiFiClientSecure *)socket->wifi_client;
    client->stop();
  }
  socket->connected = 0;
  return 0;
}

int atclient_tls_socket_write(struct atclient_tls_socket *socket,
                              const unsigned char *value, size_t value_len) {
  if (socket == NULL || value == NULL || value_len == 0) return 1;
  if (socket->wifi_client == nullptr || !socket->connected) return 1;

  WiFiClientSecure *client = (WiFiClientSecure *)socket->wifi_client;

  size_t written = 0;
  while (written < value_len) {
    size_t w = client->write(value + written, value_len - written);
    if (w == 0) {
      atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                   "Write failed after %u/%u bytes\n", (unsigned)written, (unsigned)value_len);
      return 1;
    }
    written += w;
  }

  return 0;
}

int atclient_tls_socket_read(struct atclient_tls_socket *socket,
                             unsigned char **value, size_t *value_len,
                             const struct atclient_socket_read_options options) {
  if (socket == NULL) return 1;
  if (socket->wifi_client == nullptr || !socket->connected) return 1;

  WiFiClientSecure *client = (WiFiClientSecure *)socket->wifi_client;

  // Use the pre-allocated persistent read buffer (allocated once at configure
  // time) instead of malloc/free on every call.  This prevents spurious read
  // failures when the heap is fragmented by relay TCP pbufs.
  unsigned char *buf = (unsigned char *)socket->read_buf;
  if (buf == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Read buffer not allocated\n");
    return 1;
  }

  size_t pos = 0;
  unsigned long start = millis();
  unsigned long timeout_ms = (unsigned long)socket->read_timeout_ms;

  if (options.type == ATCLIENT_SOCKET_READ_UNTIL_CHAR) {
    char until = options.until_char;
    bool have_data = false;

    while (pos < READ_BUF_SIZE - 1) {
      if (client->available()) {
        int b = client->read();
        if (b < 0) break;
        buf[pos++] = (unsigned char)b;
        if ((char)b == until) break;
        // Reset timeout on data received
        start = millis();
        have_data = true;
      } else {
        // Use longer timeout (3x) once we have partial data
        unsigned long effective_timeout = have_data ? (timeout_ms * 3) : timeout_ms;
        if ((millis() - start) > effective_timeout) {
          if (pos == 0) {
            // No data at all — clean timeout (buf is persistent, don't free)
            if (value != NULL) *value = NULL;
            if (value_len != NULL) *value_len = 0;
            return ATCLIENT_SSL_TIMEOUT_EXITCODE;
          }
          break;
        }
        delay(1);
      }
    }
  } else if (options.type == ATCLIENT_SOCKET_READ_CLEAR_AT_PROMPT) {
    // Read until we get past the @ prompt
    while (pos < READ_BUF_SIZE - 1) {
      if (client->available()) {
        int b = client->read();
        if (b < 0) break;
        buf[pos++] = (unsigned char)b;
        // Check if we've received the full prompt
        if ((char)b == '\n') break;
        start = millis();
      } else {
        if ((millis() - start) > timeout_ms) {
          if (pos == 0) {
            // No data at all — clean timeout (buf is persistent, don't free)
            if (value != NULL) *value = NULL;
            if (value_len != NULL) *value_len = 0;
            return ATCLIENT_SSL_TIMEOUT_EXITCODE;
          }
          break;
        }
        delay(1);
      }
    }
  }

  if (pos == 0) {
    // No data (buf is persistent, don't free)
    if (value != NULL) *value = NULL;
    if (value_len != NULL) *value_len = 0;
    return ATCLIENT_SSL_TIMEOUT_EXITCODE;
  }

  // If caller doesn't want the data, just discard it (buf is persistent)
  if (value == NULL) {
    if (value_len != NULL) *value_len = pos;
    return 0;
  }

  // buf is the persistent workspace — copy result into a caller-owned allocation.
  // The at-protocol response is typically small (<200 bytes), so this malloc
  // is tiny and succeeds even under heap fragmentation.  The caller frees *value.
  *value = (unsigned char *)malloc(pos + 1);
  if (*value == NULL) {
    if (value_len != NULL) *value_len = 0;
    return 1;
  }
  memcpy(*value, buf, pos);
  (*value)[pos] = '\0';
  if (value_len != NULL) *value_len = pos;

  return 0;
}

void atclient_tls_socket_set_read_timeout(struct atclient_tls_socket *socket,
                                          const int timeout_ms) {
  if (socket == NULL) return;
  socket->read_timeout_ms = timeout_ms;
  if (socket->wifi_client != nullptr) {
    WiFiClientSecure *client = (WiFiClientSecure *)socket->wifi_client;
    client->setTimeout(timeout_ms / 1000);
  }
}

// ============================================================
// Raw Socket (minimal, not commonly used on Arduino)
// ============================================================

void atclient_raw_socket_init(struct atclient_raw_socket *socket) {
  if (socket == NULL) return;
  socket->wifi_client = nullptr;
  socket->connected = 0;
}

void atclient_raw_socket_free(struct atclient_raw_socket *socket) {
  if (socket == NULL) return;
  if (socket->wifi_client != nullptr) {
    // Raw socket cleanup if needed
    socket->wifi_client = nullptr;
  }
  socket->connected = 0;
}

} // extern "C"
