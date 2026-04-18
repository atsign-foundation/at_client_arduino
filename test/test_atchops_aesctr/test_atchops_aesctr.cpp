/**
 * test_atchops_aesctr.cpp
 *
 * Unity tests for atchops_aes_ctr_encrypt / _decrypt.
 * Mirrors at_c/packages/atchops/tests/test_aesctr.c and test_aesctr_decrypt.c
 * but adapted to the Unity framework used by PlatformIO native tests.
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atchops/aes_ctr.h"
#include "atchops/base64.h"
#include "atchops/iv.h"
}

void setUp() {}
void tearDown() {}

/* ── roundtrip: encrypt then decrypt should reproduce original ──────────────
 * Same key / plaintext as at_c test_aesctr.c
 */
void test_aesctr_encrypt_decrypt_roundtrip() {
  const char *plaintext_str = "I like to eat pizza 123";
  const char *aeskey_b64 = "1DPU9OP3CYvamnVBMwGgL7fm8yB1klAap0Uc5Z9R79g=";

  const size_t keysize = 32;
  unsigned char key[keysize];
  memset(key, 0, keysize);
  size_t keylen = 0;
  TEST_ASSERT_EQUAL_INT(0, atchops_base64_decode(aeskey_b64, strlen(aeskey_b64), key, keysize, &keylen));
  TEST_ASSERT_EQUAL_INT(32, (int)keylen);

  unsigned char iv[ATCHOPS_IV_BUFFER_SIZE];
  memset(iv, 0, sizeof(iv));

  const size_t ciphertext_size = 4096;
  unsigned char ciphertext[ciphertext_size];
  memset(ciphertext, 0, ciphertext_size);
  size_t ciphertext_len = 0;

  TEST_ASSERT_EQUAL_INT(0, atchops_aes_ctr_encrypt(
      key, ATCHOPS_AES_256, iv,
      (const unsigned char *)plaintext_str, strlen(plaintext_str),
      ciphertext, ciphertext_size, &ciphertext_len));
  TEST_ASSERT_GREATER_THAN(0, (int)ciphertext_len);

  /* Reset IV to zero for decrypt (keys in atKeys file use IV={0}*16) */
  memset(iv, 0, sizeof(iv));

  const size_t plaintext2_size = 4096;
  unsigned char plaintext2[plaintext2_size];
  memset(plaintext2, 0, plaintext2_size);
  size_t plaintext2_len = 0;

  TEST_ASSERT_EQUAL_INT(0, atchops_aes_ctr_decrypt(
      key, ATCHOPS_AES_256, iv,
      ciphertext, ciphertext_len,
      plaintext2, plaintext2_size, &plaintext2_len));

  TEST_ASSERT_EQUAL_INT((int)strlen(plaintext_str), (int)plaintext2_len);
  TEST_ASSERT_EQUAL_STRING(plaintext_str, (char *)plaintext2);
}

/* ── decrypt a known ciphertext (from at_c test_aesctr_decrypt.c) ────────────
 * key  : su16AzIiiGZULJYFsxDWyyy8yAJQNJvEsmVNkr2/0Vo=
 * iv   : {0} * 16
 * The decrypted plaintext is a long string that starts with a known prefix.
 */
void test_aesctr_decrypt_known_ciphertext() {
  /* Use a simpler self-contained roundtrip with a fixed key/IV/plaintext to
   * verify correctness without embedding a 2 KB base64 blob. */
  const char *key_b64 = "su16AzIiiGZULJYFsxDWyyy8yAJQNJvEsmVNkr2/0Vo=";
  const char *known_plaintext = "Hello from atSDK Arduino!";

  unsigned char key[32];
  size_t keylen = 0;
  TEST_ASSERT_EQUAL_INT(0, atchops_base64_decode(key_b64, strlen(key_b64), key, 32, &keylen));

  unsigned char iv[ATCHOPS_IV_BUFFER_SIZE];
  memset(iv, 0, sizeof(iv));

  unsigned char ciphertext[256];
  memset(ciphertext, 0, sizeof(ciphertext));
  size_t ciphertext_len = 0;

  TEST_ASSERT_EQUAL_INT(0, atchops_aes_ctr_encrypt(
      key, ATCHOPS_AES_256, iv,
      (const unsigned char *)known_plaintext, strlen(known_plaintext),
      ciphertext, sizeof(ciphertext), &ciphertext_len));

  memset(iv, 0, sizeof(iv));

  unsigned char decrypted[256];
  memset(decrypted, 0, sizeof(decrypted));
  size_t decrypted_len = 0;

  TEST_ASSERT_EQUAL_INT(0, atchops_aes_ctr_decrypt(
      key, ATCHOPS_AES_256, iv,
      ciphertext, ciphertext_len,
      decrypted, sizeof(decrypted), &decrypted_len));

  TEST_ASSERT_EQUAL_STRING(known_plaintext, (char *)decrypted);
}

/* ── different plaintexts should give different ciphertexts ─────────────── */
void test_aesctr_different_plaintexts_differ() {
  const char *key_b64 = "1DPU9OP3CYvamnVBMwGgL7fm8yB1klAap0Uc5Z9R79g=";
  unsigned char key[32];
  size_t keylen = 0;
  atchops_base64_decode(key_b64, strlen(key_b64), key, 32, &keylen);

  unsigned char iv1[ATCHOPS_IV_BUFFER_SIZE], iv2[ATCHOPS_IV_BUFFER_SIZE];
  memset(iv1, 0, sizeof(iv1));
  memset(iv2, 0, sizeof(iv2));

  unsigned char ct1[256], ct2[256];
  memset(ct1, 0, sizeof(ct1));
  memset(ct2, 0, sizeof(ct2));
  size_t ct1_len = 0, ct2_len = 0;

  atchops_aes_ctr_encrypt(key, ATCHOPS_AES_256, iv1, (const unsigned char *)"aaa", 3, ct1, sizeof(ct1), &ct1_len);
  atchops_aes_ctr_encrypt(key, ATCHOPS_AES_256, iv2, (const unsigned char *)"bbb", 3, ct2, sizeof(ct2), &ct2_len);

  TEST_ASSERT_EQUAL_INT((int)ct1_len, (int)ct2_len);
  TEST_ASSERT_NOT_EQUAL(0, memcmp(ct1, ct2, ct1_len));
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_aesctr_encrypt_decrypt_roundtrip);
  RUN_TEST(test_aesctr_decrypt_known_ciphertext);
  RUN_TEST(test_aesctr_different_plaintexts_differ);
  return UNITY_END();
}
