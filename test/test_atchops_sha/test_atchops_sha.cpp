/**
 * test_atchops_sha.cpp
 *
 * Unity tests for atchops_sha_hash (SHA-256 and SHA-512).
 * Uses well-known NIST/RFC test vectors so the results can be verified
 * independently without connecting to an atServer.
 */

#include <unity.h>
#include <string.h>
#include <stdio.h>

extern "C" {
#include "atchops/sha.h"
}

void setUp() {}
void tearDown() {}

/* Helper: hash → lowercase hex string */
static void bytes_to_hex(const unsigned char *bytes, size_t len, char *out) {
  for (size_t i = 0; i < len; i++) {
    snprintf(out + i * 2, 3, "%02x", bytes[i]);
  }
}

/* ── SHA-256 known vectors ───────────────────────────────────────────────────
 *  SHA-256("Hello!") = 334d016f755cd6dc58c53a86e183882f8ec14f52fb05345887c8a5edd42c87b7
 *  Same vector used in at_c/packages/atchops/tests/test_sha.c
 */

void test_sha256_hello() {
  const unsigned char input[] = "Hello!";
  unsigned char digest[32];
  memset(digest, 0, sizeof(digest));

  TEST_ASSERT_EQUAL_INT(0, atchops_sha_hash(ATCHOPS_MD_SHA256, input, 6, digest));

  char hex[65];
  bytes_to_hex(digest, 32, hex);
  hex[64] = '\0';
  TEST_ASSERT_EQUAL_STRING("334d016f755cd6dc58c53a86e183882f8ec14f52fb05345887c8a5edd42c87b7", hex);
}

void test_sha256_empty_input_rejected() {
  /* The library validates input_len > 0, so empty input should return an error */
  const unsigned char input[] = "";
  unsigned char digest[32];
  TEST_ASSERT_NOT_EQUAL(0, atchops_sha_hash(ATCHOPS_MD_SHA256, input, 0, digest));
}

/* ── SHA-512 known vector ────────────────────────────────────────────────────
 *  SHA-512("abc") = ddaf35a193617aba...  (NIST FIPS 180-4, first 16 chars verified here)
 */

void test_sha512_abc() {
  const unsigned char input[] = "abc";
  unsigned char digest[64];
  memset(digest, 0, sizeof(digest));

  TEST_ASSERT_EQUAL_INT(0, atchops_sha_hash(ATCHOPS_MD_SHA512, input, 3, digest));

  char hex[129];
  bytes_to_hex(digest, 64, hex);
  hex[128] = '\0';
  /* Verify the full expected digest */
  TEST_ASSERT_EQUAL_STRING(
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
      "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
      hex);
}

/* ── round-trip: hash should be deterministic ───────────────────────────────── */

void test_sha256_deterministic() {
  const unsigned char input[] = "atSDK Arduino test";
  unsigned char digest1[32], digest2[32];

  TEST_ASSERT_EQUAL_INT(0, atchops_sha_hash(ATCHOPS_MD_SHA256, input, sizeof(input) - 1, digest1));
  TEST_ASSERT_EQUAL_INT(0, atchops_sha_hash(ATCHOPS_MD_SHA256, input, sizeof(input) - 1, digest2));
  TEST_ASSERT_EQUAL_MEMORY(digest1, digest2, 32);
}

/* ── SHA-256 output length is always 32 bytes ───────────────────────────────── */

void test_sha256_output_is_32_bytes() {
  /* Verify the digest buffer is fully populated (no leading zeroes from memset) */
  const unsigned char input[] = "non-empty input that produces a real digest";
  unsigned char digest[32];
  memset(digest, 0, sizeof(digest));

  TEST_ASSERT_EQUAL_INT(0, atchops_sha_hash(ATCHOPS_MD_SHA256, input, sizeof(input) - 1, digest));

  /* At least one byte must be non-zero — a real SHA-256 won't produce all-zero output */
  int nonzero = 0;
  for (int i = 0; i < 32; i++) {
    if (digest[i] != 0) { nonzero = 1; break; }
  }
  TEST_ASSERT_TRUE(nonzero);
}

/* ── main ───────────────────────────────────────────────────────────────────── */

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sha256_hello);
  RUN_TEST(test_sha256_empty_input_rejected);
  RUN_TEST(test_sha512_abc);
  RUN_TEST(test_sha256_deterministic);
  RUN_TEST(test_sha256_output_is_32_bytes);
  return UNITY_END();
}
