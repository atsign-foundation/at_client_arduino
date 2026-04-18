#include "atdirectory.h"
#include "atlogger/atlogger.h"
#include <string.h>
#define TAG "atdirectory"

struct atdirectory_connection atdirectory_connection_create(const char *host, const uint16_t port) {
  size_t len = strlen(host);
  char *host_clone = malloc(sizeof(char) * (len + 1));
  if (host_clone == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to allocate memory for atdirectory_host\n");
    return (struct atdirectory_connection){{}, NULL, 0};
  }
  memcpy(host_clone, host, len);
  host_clone[len] = 0;

  struct atdirectory_connection conn = {{}, host_clone, port};
  atclient_tls_socket_init(&conn.socket);
  return conn;
}

void atdirectory_connection_free(struct atdirectory_connection *conn) {
  if (conn->host != NULL) {
    free((char *)conn->host);
    conn->host = NULL;
  }
  atclient_tls_socket_free(&conn->socket);
}

int atdirectory_lookup(struct atdirectory_connection *conn, const char *atsign, char **atserver_host,
                       uint16_t *atserver_port) {
  int ret = atclient_tls_socket_configure(&conn->socket, NULL, 0);
  if (ret != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to configure tls for %s:%d", conn->host, conn->port);
    return ret;
  }

  ret = atclient_tls_socket_connect(&conn->socket, conn->host, conn->port);
  if (ret != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to connect to %s:%u\n", conn->host, conn->port);
    return ret;
  }

  uint8_t atsign_offset = 0;
  if (atsign[0] == '@') {
    atsign_offset = 1;
  }
  size_t atsign_len = strlen(atsign) - atsign_offset;
  char *send_buf = malloc(sizeof(char) * atsign_len + 1);
  if (send_buf == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to allocate memory for send_buf\n");
    return 1;
  }

  memcpy(send_buf, atsign + atsign_offset, atsign_len);
  send_buf[atsign_len] = '\n';

  ret = atclient_tls_socket_write(&conn->socket, (unsigned char *)send_buf, atsign_len + 1);
  free(send_buf);
  if (ret != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to write lookup command to atdirectory\n");
    return ret;
  }

  unsigned char *read_buf;
  size_t read_len;

  ret = atclient_tls_socket_read(&conn->socket, &read_buf, &read_len, atclient_socket_read_until_char('\n'));
  if (ret != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to read from atdirectory\n");
    return ret;
  }

  unsigned char *buf = read_buf;
  size_t len = read_len;

  // ignore starting '@' if it exists
  if (buf[0] == '@') {
    buf++;
    len--;
  }

  if (len == 4 && strncmp((char *)buf, "null", 4) == 0) {
    *atserver_host = NULL;
    *atserver_port = 0;
    free(read_buf);
    return 0;
  }

  ret = atdirectory_parse_host_port_from_buf((char *)buf, len, atserver_host, atserver_port);
  free(read_buf);

  return ret;
}

int atdirectory_lookup_once(const char *atdirectory_host, const uint16_t atdirectory_port, const char *atsign,
                            char **atserver_host, uint16_t *atserver_port) {
  struct atdirectory_connection conn = atdirectory_connection_create(atdirectory_host, atdirectory_port);
  if (conn.host == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to create the atdirectory_connection\n");
    return 1;
  }

  int ret = atdirectory_lookup(&conn, atsign, atserver_host, atserver_port);
  atdirectory_connection_free(&conn);
  return ret;
}

// TODO: unit tests
int atdirectory_parse_host_port_from_buf(const char *buf, size_t len, char **host, uint16_t *port) {
  size_t pos = 0;
  while (++pos < len && buf[pos] != ':')
    ; // walk to the ':' character

  if (pos == len) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to parse ':' in host:port buffer\n");
    return 1;
  }
  // make pos equal to the starting offset of port string AND the length of host string + 1
  pos++;

  // prepare null-terminated port buffer to parse using atoi
  len -= pos; // length of the port string now
  char *port_buf = malloc(sizeof(char) * (len + 1));
  if (port_buf == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to allocate memory for port parse string \n");
    return 1;
  }

  *host = malloc(sizeof(char) * pos);
  if (host == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to allocate memory for host string \n");
    free(port_buf);
    return 1;
  }

  memcpy(*host, buf, pos - 1);
  (*host)[pos - 1] = 0;
  memcpy(port_buf, buf + pos, len);
  port_buf[len] = 0;

  *port = atoi(port_buf);
  free(port_buf);

  if (*port == 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR, "Failed to parse port from port string\n");
    free(*host);
    *host = NULL;
    return 1;
  }

  return 0;
}
