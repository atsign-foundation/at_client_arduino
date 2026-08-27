/**
 * test_atauth_resolve_atserver.cpp
 *
 * Unity tests for atauth_resolve_atserver, which resolves a root server spec
 * following the Dart AtRootDomain / at_lookup convention:
 *
 *   host             -> ask the atDirectory at host:64
 *   host:port        -> ask the atDirectory at host:port
 *   proxy:host       -> no atDirectory: the atServer address is host:64
 *   proxy:host:port  -> no atDirectory: the atServer address is host:port
 *
 * The atDirectory lookup is stubbed in test_sources.c so the non-proxy paths
 * can assert which host:port would be asked, without a live atDirectory.
 * Port validation must use strtol semantics: atoi + uint16_t cast silently
 * wraps out-of-range ports (70000 -> 4464, -1 -> 65535) and accepts trailing
 * garbage (443x).
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atauth/resolve_atserver.h"

/* stub state, defined in test_sources.c */
extern int stub_find_calls;
extern char stub_find_host[300];
extern uint16_t stub_find_port;
extern int stub_find_ret;
}

void setUp() {
  stub_find_calls = 0;
  stub_find_host[0] = '\0';
  stub_find_port = 0;
  stub_find_ret = 0;
}
void tearDown() {}

static int resolve(const char *spec, char **host, uint16_t *port) {
  return atauth_resolve_atserver(spec, "@alice", host, port);
}

/* ── proxy: forms — no atDirectory lookup ────────────────────────────────── */

void test_proxy_host_port() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, resolve("proxy:proxy0001.atsign.org:443", &host, &port));
  TEST_ASSERT_EQUAL_STRING("proxy0001.atsign.org", host);
  TEST_ASSERT_EQUAL_UINT16(443, port);
  TEST_ASSERT_EQUAL_INT(0, stub_find_calls);
  free(host);
}

void test_proxy_host_default_port() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, resolve("proxy:proxy0001.atsign.org", &host, &port));
  TEST_ASSERT_EQUAL_STRING("proxy0001.atsign.org", host);
  TEST_ASSERT_EQUAL_UINT16(64, port);
  TEST_ASSERT_EQUAL_INT(0, stub_find_calls);
  free(host);
}

/* ── atDirectory forms — lookup via the stub ─────────────────────────────── */

void test_bare_host_asks_directory_on_64() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, resolve("root.atsign.org", &host, &port));
  TEST_ASSERT_EQUAL_INT(1, stub_find_calls);
  TEST_ASSERT_EQUAL_STRING("root.atsign.org", stub_find_host);
  TEST_ASSERT_EQUAL_UINT16(64, stub_find_port);
  TEST_ASSERT_EQUAL_STRING("lookedup.atsign.zone", host);
  TEST_ASSERT_EQUAL_UINT16(6464, port);
  free(host);
}

void test_host_port_asks_directory_on_that_port() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, resolve("vip.ve.atsign.zone:8464", &host, &port));
  TEST_ASSERT_EQUAL_INT(1, stub_find_calls);
  TEST_ASSERT_EQUAL_STRING("vip.ve.atsign.zone", stub_find_host);
  TEST_ASSERT_EQUAL_UINT16(8464, stub_find_port);
  free(host);
}

void test_directory_failure_propagates() {
  char *host = NULL;
  uint16_t port = 0;
  stub_find_ret = 1;
  TEST_ASSERT_NOT_EQUAL(0, resolve("root.atsign.org", &host, &port));
  TEST_ASSERT_NULL(host);
}

/* ── invalid specs ───────────────────────────────────────────────────────── */

void test_reject_null_and_empty_spec() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, atauth_resolve_atserver(NULL, "@alice", &host, &port));
  TEST_ASSERT_NOT_EQUAL(0, resolve("", &host, &port));
  TEST_ASSERT_NULL(host);
  TEST_ASSERT_EQUAL_INT(0, stub_find_calls);
}

void test_reject_proxy_with_no_host() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve("proxy:", &host, &port));
  TEST_ASSERT_NULL(host);
  TEST_ASSERT_EQUAL_INT(0, stub_find_calls);
}

void test_reject_empty_host_before_colon() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve(":443", &host, &port));
  TEST_ASSERT_NULL(host);
  TEST_ASSERT_EQUAL_INT(0, stub_find_calls);
}

void test_reject_port_zero() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve("proxy:h:0", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_port_out_of_range() {
  /* atoi + uint16_t cast used to wrap 70000 -> 4464 */
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve("proxy:h:70000", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_negative_port() {
  /* atoi + uint16_t cast used to wrap -1 -> 65535 */
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve("proxy:h:-1", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_trailing_garbage_port() {
  /* atoi used to accept 443x as 443 */
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve("proxy:h:443x", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_empty_port() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve("proxy:h:", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_directory_port_out_of_range() {
  /* the non-proxy form must be validated the same way, before any lookup */
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve("root.atsign.org:70000", &host, &port));
  TEST_ASSERT_NULL(host);
  TEST_ASSERT_EQUAL_INT(0, stub_find_calls);
}

void test_reject_overlong_host() {
  char spec[300];
  memset(spec, 'a', sizeof(spec) - 1);
  spec[sizeof(spec) - 1] = '\0';
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, resolve(spec, &host, &port));
  TEST_ASSERT_NULL(host);
  TEST_ASSERT_EQUAL_INT(0, stub_find_calls);
}

void test_max_port_accepted() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, resolve("proxy:h:65535", &host, &port));
  TEST_ASSERT_EQUAL_UINT16(65535, port);
  free(host);
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_proxy_host_port);
  RUN_TEST(test_proxy_host_default_port);
  RUN_TEST(test_bare_host_asks_directory_on_64);
  RUN_TEST(test_host_port_asks_directory_on_that_port);
  RUN_TEST(test_directory_failure_propagates);
  RUN_TEST(test_reject_null_and_empty_spec);
  RUN_TEST(test_reject_proxy_with_no_host);
  RUN_TEST(test_reject_empty_host_before_colon);
  RUN_TEST(test_reject_port_zero);
  RUN_TEST(test_reject_port_out_of_range);
  RUN_TEST(test_reject_negative_port);
  RUN_TEST(test_reject_trailing_garbage_port);
  RUN_TEST(test_reject_empty_port);
  RUN_TEST(test_reject_directory_port_out_of_range);
  RUN_TEST(test_reject_overlong_host);
  RUN_TEST(test_max_port_accepted);
  return UNITY_END();
}
