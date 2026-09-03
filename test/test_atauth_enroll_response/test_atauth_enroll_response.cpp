/**
 * test_atauth_enroll_response.cpp
 *
 * Unity tests for parse_enrollment_response.
 * Malformed-response cases taken from at_c#715: well-formed JSON with missing
 * or non-string mandatory fields must fail, not return 0 with NULL fields the
 * callers strlen unconditionally.
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atauth/enroll_response.h"
}

void setUp() {}
void tearDown() {}

/* ── 1a. well-formed response parses ─────────────────────────────────────── */
void test_parse_valid_response() {
  struct enroll_response response;
  const char *buffer = "data:{\"enrollmentId\":\"abc-123\",\"status\":\"pending\"}";

  TEST_ASSERT_EQUAL_INT(0, parse_enrollment_response(buffer, &response));
  TEST_ASSERT_NOT_NULL(response.enrollment_id);
  TEST_ASSERT_NOT_NULL(response.status);
  TEST_ASSERT_EQUAL_STRING("abc-123", response.enrollment_id);
  TEST_ASSERT_EQUAL_STRING("pending", response.status);
  free_enroll_response(&response);
}

/* ── 1b. missing or non-string mandatory fields must fail ────────────────── */
void test_parse_malformed_responses() {
  const char *malformed[] = {
      "data:{\"status\":\"pending\"}",                     // missing enrollmentId
      "data:{\"enrollmentId\":\"abc\"}",                   // missing status
      "data:{\"enrollmentId\":42,\"status\":\"pending\"}", // non-string enrollmentId
      "data:{\"enrollmentId\":\"abc\",\"status\":null}",   // non-string status
  };
  for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); i++) {
    struct enroll_response response;
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, parse_enrollment_response(malformed[i], &response), malformed[i]);
  }
}

/* ── 1c. error: and garbage responses must fail ──────────────────────────── */
void test_parse_error_responses() {
  struct enroll_response response;
  TEST_ASSERT_NOT_EQUAL(0, parse_enrollment_response("error:AT0011: invalid otp", &response));
  TEST_ASSERT_NOT_EQUAL(0, parse_enrollment_response("garbage", &response));
  TEST_ASSERT_NOT_EQUAL(0, parse_enrollment_response("data:not json", &response));
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid_response);
  RUN_TEST(test_parse_malformed_responses);
  RUN_TEST(test_parse_error_responses);
  return UNITY_END();
}
