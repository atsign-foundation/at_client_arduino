/**
 * test_atchops_uuid.cpp
 *
 * Unity tests for atchops_uuid_generate.
 * Mirrors at_c/packages/atchops/tests/test_uuid4.c
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atchops/uuid.h"
}

void setUp() {}
void tearDown() {}

/* ── UUID is non-empty and 36 characters ──────────────────────────────────── */
void test_uuid_generate_length() {
  char dst[37];
  memset(dst, 0, sizeof(dst));

  TEST_ASSERT_EQUAL_INT(0, atchops_uuid_init());
  atchops_uuid_generate(dst, sizeof(dst));

  TEST_ASSERT_EQUAL_INT(36, (int)strlen(dst));
}

/* ── UUID has hyphens in the right positions (8-4-4-4-12) ─────────────────── */
void test_uuid_format() {
  char dst[37];
  memset(dst, 0, sizeof(dst));

  atchops_uuid_init();
  atchops_uuid_generate(dst, sizeof(dst));

  TEST_ASSERT_EQUAL_INT('-', dst[8]);
  TEST_ASSERT_EQUAL_INT('-', dst[13]);
  TEST_ASSERT_EQUAL_INT('-', dst[18]);
  TEST_ASSERT_EQUAL_INT('-', dst[23]);
}

/* ── Two consecutive UUIDs should differ ─────────────────────────────────── */
void test_uuid_uniqueness() {
  char uuid1[37], uuid2[37];
  memset(uuid1, 0, sizeof(uuid1));
  memset(uuid2, 0, sizeof(uuid2));

  atchops_uuid_init();
  atchops_uuid_generate(uuid1, sizeof(uuid1));
  atchops_uuid_generate(uuid2, sizeof(uuid2));

  TEST_ASSERT_NOT_EQUAL(0, strcmp(uuid1, uuid2));
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_uuid_generate_length);
  RUN_TEST(test_uuid_format);
  RUN_TEST(test_uuid_uniqueness);
  return UNITY_END();
}
