/*
 * test_sources.c — compiled as C (not C++) so that C-only constructs work.
 * PlatformIO compiles all files in a test directory into one binary.
 *
 * atclient_utils_find_atserver_address is stubbed below (instead of
 * including atclient_atclient_utils.c) so the non-proxy paths can be
 * tested without a live atDirectory.
 */
#include "../../src/atlogger_atlogger.c"
#include "../../src/atauth_resolve_atserver.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── atDirectory lookup stub ─────────────────────────────────────────────── */

int stub_find_calls = 0;
char stub_find_host[300];
uint16_t stub_find_port = 0;
int stub_find_ret = 0;

int atclient_utils_find_atserver_address(const char *atdirectory_host, const uint16_t atdirectory_port,
                                         const char *atsign, char **atserver_host, uint16_t *atserver_port) {
  (void)atsign;
  stub_find_calls++;
  snprintf(stub_find_host, sizeof(stub_find_host), "%s", atdirectory_host);
  stub_find_port = atdirectory_port;
  if (stub_find_ret != 0) {
    return stub_find_ret;
  }
  const char *resolved = "lookedup.atsign.zone";
  *atserver_host = malloc(strlen(resolved) + 1);
  if (*atserver_host == NULL) {
    return 1;
  }
  memcpy(*atserver_host, resolved, strlen(resolved) + 1);
  *atserver_port = 6464;
  return 0;
}
