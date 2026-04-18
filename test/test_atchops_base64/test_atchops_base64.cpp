/**
 * test_atchops_base64.cpp
 *
 * Unity tests for atchops_base64_encode / atchops_base64_decode.
 * Mirrors the style of at_c functional tests but runs natively (no hardware).
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atchops/base64.h"
}

void setUp() {}
void tearDown() {}

/* ── round-trip ────────────────────────────────────────────────────────────── */

void test_encode_decode_roundtrip() {
  const unsigned char input[] = "Hello, atSDK on Arduino!";
  const size_t input_len = sizeof(input) - 1;

  const size_t enc_size = atchops_base64_encoded_size(input_len);
  char *encoded = (char *)malloc(enc_size);
  TEST_ASSERT_NOT_NULL(encoded);

  size_t encoded_len = 0;
  TEST_ASSERT_EQUAL_INT(0, atchops_base64_encode(input, input_len, encoded, enc_size, &encoded_len));
  TEST_ASSERT_GREATER_THAN(0, (int)encoded_len);

  const size_t dec_size = atchops_base64_decoded_size(encoded_len);
  unsigned char *decoded = (unsigned char *)malloc(dec_size);
  TEST_ASSERT_NOT_NULL(decoded);

  size_t decoded_len = 0;
  TEST_ASSERT_EQUAL_INT(0, atchops_base64_decode(encoded, encoded_len, decoded, dec_size, &decoded_len));
  TEST_ASSERT_EQUAL_size_t(input_len, decoded_len);
  TEST_ASSERT_EQUAL_MEMORY(input, decoded, input_len);

  free(encoded);
  free(decoded);
}

/* ── known-value ("Hello" → "SGVsbG8=") ────────────────────────────────────── */

void test_encode_known_value() {
  const unsigned char input[] = "Hello";
  const size_t input_len = 5;

  char encoded[32];
  memset(encoded, 0, sizeof(encoded));

  TEST_ASSERT_EQUAL_INT(0, atchops_base64_encode(input, input_len, encoded, sizeof(encoded), NULL));
  TEST_ASSERT_EQUAL_STRING("SGVsbG8=", encoded);
}

void test_decode_known_value() {
  const char *input = "SGVsbG8=";
  unsigned char decoded[32];
  memset(decoded, 0, sizeof(decoded));
  size_t decoded_len = 0;

  TEST_ASSERT_EQUAL_INT(0, atchops_base64_decode(input, strlen(input), decoded, sizeof(decoded), &decoded_len));
  TEST_ASSERT_EQUAL_size_t(5, decoded_len);
  TEST_ASSERT_EQUAL_MEMORY("Hello", decoded, 5);
}

/* ── empty/single byte ──────────────────────────────────────────────────────── */

void test_encode_single_byte() {
  const unsigned char input[] = {0xFF};
  char encoded[8];
  size_t encoded_len = 0;

  TEST_ASSERT_EQUAL_INT(0, atchops_base64_encode(input, 1, encoded, sizeof(encoded), &encoded_len));
  TEST_ASSERT_EQUAL_STRING("/w==", encoded);
}

/* ── NULL argument validation ───────────────────────────────────────────────── */

void test_encode_null_src_fails() {
  char dst[32];
  size_t len = 0;
  TEST_ASSERT_NOT_EQUAL(0, atchops_base64_encode(NULL, 5, dst, sizeof(dst), &len));
}

void test_decode_null_src_fails() {
  unsigned char dst[32];
  size_t len = 0;
  TEST_ASSERT_NOT_EQUAL(0, atchops_base64_decode(NULL, 4, dst, sizeof(dst), &len));
}

/* ── main ───────────────────────────────────────────────────────────────────── */

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_encode_decode_roundtrip);
  RUN_TEST(test_encode_known_value);
  RUN_TEST(test_decode_known_value);
  RUN_TEST(test_encode_single_byte);
  RUN_TEST(test_encode_null_src_fails);
  RUN_TEST(test_decode_null_src_fails);
  return UNITY_END();
}
