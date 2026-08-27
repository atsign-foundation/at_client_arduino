/**
 * test_atclient_atdirectory.cpp
 *
 * Unity tests for atdirectory_parse_host_port_from_buf, in particular the
 * port validation: atoi-style parsing silently wraps out-of-range ports
 * (70000 -> 4464, -1 -> 65535) and accepts trailing garbage (443x).
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atdirectory.h"
}

void setUp() {}
void tearDown() {}

static int parse(const char *str, char **host, uint16_t *port) {
  return atdirectory_parse_host_port_from_buf(str, strlen(str), host, port);
}

/* ── valid specs ─────────────────────────────────────────────────────────── */

void test_parse_valid_host_port() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, parse("foo.atsign.zone:6464", &host, &port));
  TEST_ASSERT_EQUAL_STRING("foo.atsign.zone", host);
  TEST_ASSERT_EQUAL_UINT16(6464, port);
  free(host);
}

void test_parse_trailing_lf() {
  /* the directory response is read up to and including the '\n' */
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, parse("foo.atsign.zone:6464\n", &host, &port));
  TEST_ASSERT_EQUAL_STRING("foo.atsign.zone", host);
  TEST_ASSERT_EQUAL_UINT16(6464, port);
  free(host);
}

void test_parse_trailing_crlf() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, parse("foo.atsign.zone:6464\r\n", &host, &port));
  TEST_ASSERT_EQUAL_STRING("foo.atsign.zone", host);
  TEST_ASSERT_EQUAL_UINT16(6464, port);
  free(host);
}

void test_parse_max_port() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_EQUAL_INT(0, parse("h:65535", &host, &port));
  TEST_ASSERT_EQUAL_UINT16(65535, port);
  free(host);
}

/* ── invalid specs ───────────────────────────────────────────────────────── */

void test_reject_port_out_of_range() {
  /* atoi + uint16_t cast used to wrap 70000 -> 4464 */
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, parse("foo.atsign.zone:70000", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_negative_port() {
  /* atoi + uint16_t cast used to wrap -1 -> 65535 */
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, parse("foo.atsign.zone:-1", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_trailing_garbage() {
  /* atoi used to accept 443x as 443 */
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, parse("foo.atsign.zone:443x", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_port_zero() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, parse("foo.atsign.zone:0", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_empty_port() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, parse("foo.atsign.zone:", &host, &port));
  TEST_ASSERT_NULL(host);
}

void test_reject_missing_colon() {
  char *host = NULL;
  uint16_t port = 0;
  TEST_ASSERT_NOT_EQUAL(0, parse("foo.atsign.zone", &host, &port));
  TEST_ASSERT_NULL(host);
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid_host_port);
  RUN_TEST(test_parse_trailing_lf);
  RUN_TEST(test_parse_trailing_crlf);
  RUN_TEST(test_parse_max_port);
  RUN_TEST(test_reject_port_out_of_range);
  RUN_TEST(test_reject_negative_port);
  RUN_TEST(test_reject_trailing_garbage);
  RUN_TEST(test_reject_port_zero);
  RUN_TEST(test_reject_empty_port);
  RUN_TEST(test_reject_missing_colon);
  return UNITY_END();
}
