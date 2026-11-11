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

// PEM fallback bundle — used when the ESP-IDF cert bundle is not available.
// ATSDK_TLS_MINIMAL_CERTS: 3-root subset covering atSign's known CAs (~5KB heap).
// Default (no flag): full 8-root bundle (~15KB heap).
#ifdef ATSDK_TLS_MINIMAL_CERTS
static const char atclient_cacerts_pem[] =
  LETS_ENCRYPT_ROOT
  GOOGLE_GTS_ROOT_R1
  GLOBALSIGN_ROOT_CA;
#else
static const char atclient_cacerts_pem[] =
  LETS_ENCRYPT_ROOT
  GOOGLE_GLOBAL_SIGN
  GLOBALSIGN_ROOT_CA
  GOOGLE_GTS_ROOT_R1
  GOOGLE_GTS_ROOT_R2
  GOOGLE_GTS_ROOT_R3
  GOOGLE_GTS_ROOT_R4
  ZEROSSL_INTERMEDIATE;
#endif

#include <WiFiClientSecure.h>
#include <esp_arduino_version.h>
#include <lwip/sockets.h>  // setsockopt, SO_KEEPALIVE, TCP_KEEP*
#include <cstring>
#include <cstdlib>

#define TAG "atsdk_socket"

// Default read timeout (5 seconds)
#define DEFAULT_READ_TIMEOUT_MS 5000

// Max read buffer size
#define READ_BUF_SIZE (4 * 1024)

// TCP keepalive on every TLS socket. A peer that vanishes without a FIN or
// RST (NAT table expiry, access point restart, atServer host failure) is
// otherwise never noticed: lwIP keeps the socket ESTABLISHED for ever and
// every read just times out. With probes the stack errors the socket within
// about a minute, mbedtls reports that, and the caller reconnects.
#define ATSDK_TCP_KEEPIDLE_S  30
#define ATSDK_TCP_KEEPINTVL_S 10
#define ATSDK_TCP_KEEPCNT     3

// A TLS handshake that stalls (half-open peer, overloaded server) blocks the
// calling task for the framework's default 120 s, past any task watchdog.
#define ATSDK_TLS_HANDSHAKE_TIMEOUT_S 20

static void _set_tcp_keepalive(WiFiClientSecure *client) {
  int fd = client->fd();
  if (fd < 0) return;
  int yes = 1;
  int idle = ATSDK_TCP_KEEPIDLE_S, intvl = ATSDK_TCP_KEEPINTVL_S, cnt = ATSDK_TCP_KEEPCNT;
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
}

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
  socket->read_buf_len = 0;
}

int atclient_tls_socket_configure(struct atclient_tls_socket *socket,
                                  unsigned char *ca_pem, size_t ca_pem_len) {
  if (socket == NULL) return 1;

  WiFiClientSecure *client = new WiFiClientSecure();
  if (client == nullptr) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to allocate WiFiClientSecure\n");
    return 1;
  }

  // Certificate verification.
  // Prefer the ESP-IDF full Mozilla CA bundle when available: ~130 roots stored
  // in flash, zero heap cost, updates with the framework. Automatically used
  // when CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y is set in sdkconfig (no extra
  // build flag needed). Falls back to the embedded PEM bundle otherwise.
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
  extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  // Core 3.x requires the bundle size alongside the pointer.
  extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");
  client->setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
#else
  client->setCACertBundle(x509_crt_bundle_start);
#endif
#else
  // Use provided CA or the built-in atclient PEM certs
  if (ca_pem != NULL && ca_pem_len > 0) {
    client->setCACert((const char *)ca_pem);
  } else {
    client->setCACert((const char *)atclient_cacerts_pem);
  }
#endif

  client->setTimeout(socket->read_timeout_ms / 1000);
  client->setHandshakeTimeout(ATSDK_TLS_HANDSHAKE_TIMEOUT_S);

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

  socket->read_buf_len = 0;
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
  socket->read_buf_len = 0;
  if (socket->wifi_client != nullptr) {
    WiFiClientSecure *client = (WiFiClientSecure *)socket->wifi_client;
    // stop() closes the socket if it is still open and frees the TLS
    // contexts whether or not it is; connected() would only add a probe.
    client->stop();
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

  _set_tcp_keepalive(client);
  socket->read_buf_len = 0;
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
  socket->read_buf_len = 0;
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
  if (value != NULL) *value = NULL;
  if (value_len != NULL) *value_len = 0;
  if (socket == NULL) return 1;
  if (socket->wifi_client == nullptr || !socket->connected) return 1;

  WiFiClientSecure *client = (WiFiClientSecure *)socket->wifi_client;

  // The persistent read buffer, allocated once at configure time, so a read
  // never has to malloc while relay traffic has the heap fragmented.
  unsigned char *buf = (unsigned char *)socket->read_buf;
  if (buf == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Read buffer not allocated\n");
    return 1;
  }

  // Both read modes stop at a terminator: the one asked for, or the '\n' that
  // ends the line carrying the prompt.
  const char until = (options.type == ATCLIENT_SOCKET_READ_UNTIL_CHAR) ? options.until_char : '\n';

  // A TLS session the stack has already ended (peer close, reset, keepalive
  // probes unanswered) can never deliver a line: say so now, rather than
  // letting the caller wait out a timeout on every read for ever.
  if (!client->connected()) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_DEBUG, "Connection is closed\n");
    socket->read_buf_len = 0;
    return 1;
  }

  // Bytes an earlier read left behind: the start of a line it timed out
  // inside of, or what followed the terminator it stopped at. They stay in
  // the buffer so a message split across TCP segments is returned whole,
  // not as two fragments that each fail to parse.
  size_t pos = socket->read_buf_len;
  size_t scanned = 0;
  size_t line_len = 0; // 0 until the terminator is found
  bool have_data = pos > 0;
  unsigned long start = millis();
  const unsigned long timeout_ms = (unsigned long)socket->read_timeout_ms;

  for (;;) {
    for (; scanned < pos; scanned++) {
      if ((char)buf[scanned] == until) {
        line_len = scanned + 1;
        break;
      }
    }
    if (line_len > 0) break;

    if (pos >= READ_BUF_SIZE - 1) {
      // No terminator within a full buffer: the line can't be returned whole,
      // and its remainder would be taken for the next message. Drop it and
      // fail the read, so the caller resets the connection, the only sure
      // way back into sync with the stream.
      atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                   "Discarding a line of over %u bytes with no terminator\n", (unsigned)pos);
      socket->read_buf_len = 0;
      return 1;
    }

    int avail = client->available();
    if (avail < 0) {
      // available() reports the TLS session ending, and has closed it
      atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_DEBUG, "Connection closed while reading\n");
      socket->read_buf_len = 0;
      return 1;
    }
    if (avail > 0) {
      size_t room = READ_BUF_SIZE - 1 - pos;
      size_t want = (size_t)avail < room ? (size_t)avail : room;
      int n = client->read(buf + pos, want);
      if (n < 0) {
        atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_DEBUG, "Connection closed while reading\n");
        socket->read_buf_len = 0;
        return 1;
      }
      if (n > 0) {
        pos += (size_t)n;
        have_data = true;
        start = millis(); // the timeout runs from the last byte received
      }
      continue;
    }

    // Nothing waiting. Allow three times the timeout once part of a line is
    // in, as a message split across segments on a lossy link arrives in
    // pieces; a clean timeout keeps what has arrived for the next read.
    unsigned long effective_timeout = have_data ? (timeout_ms * 3) : timeout_ms;
    if ((millis() - start) > effective_timeout) {
      socket->read_buf_len = pos;
      return ATCLIENT_SSL_TIMEOUT_EXITCODE;
    }
    delay(1);
  }

  // Hand the line out, and keep whatever followed its terminator.
  if (value != NULL) {
    // The at-protocol line is typically small (<200 bytes), so this malloc
    // is tiny and succeeds even under heap fragmentation. The caller frees it.
    *value = (unsigned char *)malloc(line_len + 1);
    if (*value == NULL) {
      socket->read_buf_len = pos; // keep the line for the next read
      return 1;
    }
    memcpy(*value, buf, line_len);
    (*value)[line_len] = '\0';
  }
  if (value_len != NULL) *value_len = line_len;

  size_t leftover = pos - line_len;
  if (leftover > 0) memmove(buf, buf + line_len, leftover);
  socket->read_buf_len = leftover;
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
