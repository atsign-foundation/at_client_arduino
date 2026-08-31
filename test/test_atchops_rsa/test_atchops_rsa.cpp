/**
 * test_atchops_rsa.cpp
 *
 * Unity tests for RSA sign/verify and encrypt/decrypt.
 * Key material taken directly from at_c test_rsasign.c / test_rsaverify.c /
 * test_rsaencrypt.c / test_rsadecrypt.c.
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atchops/rsa.h"
#include "atchops/rsa_key.h"
#include "atchops/base64.h"
}

void setUp() {}
void tearDown() {}

/* ── Keys from at_c/test_rsaverify.c (sign+verify test) ─────────────────── */
#define SIGN_VERIFY_PUBLIC_KEY_B64 \
  "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuA7KmWrIPcKTH3hSzsDZ" \
  "ys60kxaqKgHeTuGCwgzHSi2SkMV9iYBCd2//KagWVoUKWyuI2KOQo2WFslqFOjOs" \
  "j4NqlhM3EamBqZqVFI5IUHtTNbXcea3igp4nVWoHwPHyJNiMf0I0rHSUeiuAbeR5" \
  "6lYGE8b64fXzKkRyQ7YRVyrp7HtYRrmA27xGDtAKT9mr+0T+5lzsHR1YLT3capll" \
  "nlUf5w8p6l8DJ4qGnODpG4/gOggEr6H5/aAVjGn8WGuRY9d7SZLJfeYG+vJ1/o/P" \
  "37L5/7sc2D6kYS1Qh45EN0jUF8ILRUJRNFKZpqWDn64LKHKCwKz00NT6OANUHNdF" \
  "awIDAQAB"

#define SIGN_VERIFY_PRIVATE_KEY_B64 \
  "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQC4DsqZasg9wpMf" \
  "eFLOwNnKzrSTFqoqAd5O4YLCDMdKLZKQxX2JgEJ3b/8pqBZWhQpbK4jYo5CjZYWy" \
  "WoU6M6yPg2qWEzcRqYGpmpUUjkhQe1M1tdx5reKCnidVagfA8fIk2Ix/QjSsdJR6" \
  "K4Bt5HnqVgYTxvrh9fMqRHJDthFXKunse1hGuYDbvEYO0ApP2av7RP7mXOwdHVgt" \
  "PdxqmWWeVR/nDynqXwMnioac4Okbj+A6CASvofn9oBWMafxYa5Fj13tJksl95gb6" \
  "8nX+j8/fsvn/uxzYPqRhLVCHjkQ3SNQXwgtFQlE0UpmmpYOfrgsocoLArPTQ1Po4" \
  "A1Qc10VrAgMBAAECggEBAJ+FMlKFGcdtO9WqkxpeSmRbgmV430JJHEOBb7J/ILpJ" \
  "hR20DHl/kBu0FZIk/DdAVxltQc2A9XqoIpfRnGY1Ivm/DEHFpZTJNHeqYkrOhh46" \
  "xINoew16hzZtm+mLW+z9xL/qbtpcpwpQf97ilQypWICgzeOWMRpl77pSWDYXNjA0" \
  "qQP/Y6A2ocAYBVkBz/SGtpWBRKb/CouzvDUqiDAa0EEKTu/Ywa9yz2GGd+eUxk2F" \
  "sM20zTmZfUipaIWkrS0bOevAcUvcVaX9Ydq1vKXa34+oZcNQqW/sS4+B+RM3ogl8" \
  "Lg2PCLYQG7azSSFHCJPwG5u7RyMWjLXwPrVkT1Nj3wECgYEA2guA5gMp7LAtmH1k" \
  "PFOz2kENveBYW1o/xzgvaBlLCZUubQCE55zlbyAMQUtEuzkyz8dT6SDLkllsDASt" \
  "30et9g8rDqa2//gK3O/mfSsx64pOqRXgr0Fb1OmHP0ESGoj3xt4pS8wENtUiwfeA" \
  "U2j+NPU4t9Y1iqslVsBJVIImLpcCgYEA2BjBcWZ9QNmFbE0rlX3+G3GQH4amc5RJ" \
  "2AUt6EyIexAZNr+1cpTGKswePw3EWwQz0sbC/Fci4qfsR5+d4rfcJp75mSV3HHEl" \
  "Z6m7iH50zKIVePSRrlgMzOPmpEoo2VelLFVx/sP70s4sPNZyKQZngn78MgtcOqQK" \
  "jTtIU96JDk0CgYEAiLLTkeCD5Tair0pVkBit1fQY6GSBIGyZNY288teAmrZjT8UW" \
  "jZpooN2HsVu98F6ww2Dk83AzEEJtoa9BTo1Cu9PQm7PbYOih7tecOfbdqhygqhLk" \
  "NRuVtgreVsK11dru9EeNvk5eif3fd5lyY1icnpjqgR6TnKcllpigoJGj3GsCgYA0" \
  "mHnkruxHb2oA/RthjEPfzBknAy/aK7p5YHFW++GwCjAI2kpAdCNzYTDvadtjx7cR" \
  "Ux08K70q63If0KKt/tAPelwHwU2nV4aiH3asdxLYh46wXN5kT7v11nZZgE9G7wUd" \
  "sEJJnsvY+CNeP1eT0qI46c1aJNey0iBbVZV6DEzRdQKBgQCPtEKSHGDom9bTErpL" \
  "TJ//6ZIrUlS+5mpCIOTgA1lyTORq9Xe+qMD7FbFQNDdlNuXtBKvwu5vYJ6Ib+VIt" \
  "sDFCwDRQsGFXNkdnSFZovMfmNQp+p6fuOgrnuSfLR1gI8nV3JQy8U/eZT5ABh06j" \
  "3A6sUy0M7TXTd6ljRS3MRBatOA=="

#define RSA_MESSAGE "_4a160d33-0c63-4800-bee0-ee254752f8c8@jeremy_0:6c987cc1-0dde-4ba1-af56-a9677086182"

/* ── RSA sign + verify roundtrip ─────────────────────────────────────────── */
void test_rsa_sign_verify_roundtrip() {
  atchops_rsa_key_private_key privkey;
  atchops_rsa_key_private_key_init(&privkey);
  atchops_rsa_key_public_key pubkey;
  atchops_rsa_key_public_key_init(&pubkey);

  TEST_ASSERT_EQUAL_INT(0, atchops_rsa_key_populate_private_key(
      &privkey, SIGN_VERIFY_PRIVATE_KEY_B64, strlen(SIGN_VERIFY_PRIVATE_KEY_B64)));
  TEST_ASSERT_EQUAL_INT(0, atchops_rsa_key_populate_public_key(
      &pubkey, SIGN_VERIFY_PUBLIC_KEY_B64, strlen(SIGN_VERIFY_PUBLIC_KEY_B64)));

  unsigned char signature[256];
  memset(signature, 0, sizeof(signature));

  TEST_ASSERT_EQUAL_INT(0, atchops_rsa_sign(
      &privkey, ATCHOPS_MD_SHA256,
      (const unsigned char *)RSA_MESSAGE, strlen(RSA_MESSAGE),
      signature));

  TEST_ASSERT_EQUAL_INT(0, atchops_rsa_verify(
      &pubkey, ATCHOPS_MD_SHA256,
      (const unsigned char *)RSA_MESSAGE, strlen(RSA_MESSAGE),
      signature));

  atchops_rsa_key_private_key_free(&privkey);
  atchops_rsa_key_public_key_free(&pubkey);
}

/* ── tampered message must fail verification ─────────────────────────────── */
void test_rsa_verify_tampered_message_fails() {
  atchops_rsa_key_private_key privkey;
  atchops_rsa_key_private_key_init(&privkey);
  atchops_rsa_key_public_key pubkey;
  atchops_rsa_key_public_key_init(&pubkey);

  atchops_rsa_key_populate_private_key(&privkey, SIGN_VERIFY_PRIVATE_KEY_B64, strlen(SIGN_VERIFY_PRIVATE_KEY_B64));
  atchops_rsa_key_populate_public_key(&pubkey, SIGN_VERIFY_PUBLIC_KEY_B64, strlen(SIGN_VERIFY_PUBLIC_KEY_B64));

  unsigned char signature[256];
  memset(signature, 0, sizeof(signature));
  atchops_rsa_sign(&privkey, ATCHOPS_MD_SHA256,
      (const unsigned char *)RSA_MESSAGE, strlen(RSA_MESSAGE), signature);

  const char *tampered = "tampered_message";
  int ret = atchops_rsa_verify(&pubkey, ATCHOPS_MD_SHA256,
      (const unsigned char *)tampered, strlen(tampered), signature);
  TEST_ASSERT_NOT_EQUAL(0, ret);

  atchops_rsa_key_private_key_free(&privkey);
  atchops_rsa_key_public_key_free(&pubkey);
}

/* ── RSA encrypt + decrypt roundtrip ────────────────────────────────────── */
/* Reuse the sign/verify keypair — already proven to load correctly above. */
void test_rsa_encrypt_decrypt_roundtrip() {
  atchops_rsa_key_public_key pubkey;
  atchops_rsa_key_public_key_init(&pubkey);
  atchops_rsa_key_private_key privkey;
  atchops_rsa_key_private_key_init(&privkey);

  TEST_ASSERT_EQUAL_INT(0, atchops_rsa_key_populate_public_key(
      &pubkey, SIGN_VERIFY_PUBLIC_KEY_B64, strlen(SIGN_VERIFY_PUBLIC_KEY_B64)));
  TEST_ASSERT_EQUAL_INT(0, atchops_rsa_key_populate_private_key(
      &privkey, SIGN_VERIFY_PRIVATE_KEY_B64, strlen(SIGN_VERIFY_PRIVATE_KEY_B64)));

  const char *plaintext = "banana";
  const size_t plaintext_len = strlen(plaintext);
  unsigned char ciphertext[256];
  memset(ciphertext, 0, sizeof(ciphertext));

  size_t ciphertext_len = 0;
  TEST_ASSERT_EQUAL_INT(0, atchops_rsa_encrypt(
      &pubkey, (const unsigned char *)plaintext, plaintext_len, ciphertext, sizeof(ciphertext), &ciphertext_len));
  TEST_ASSERT_EQUAL_size_t(256, ciphertext_len);

  // A ciphertext buffer smaller than the key's modulus must be rejected
  // without writing anything (at_c issue #701)
  unsigned char small_buffer[64];
  TEST_ASSERT_NOT_EQUAL(0, atchops_rsa_encrypt(
      &pubkey, (const unsigned char *)plaintext, plaintext_len, small_buffer, sizeof(small_buffer), NULL));

  unsigned char decrypted[256];
  memset(decrypted, 0, sizeof(decrypted));
  size_t decrypted_len = 0;

  TEST_ASSERT_EQUAL_INT(0, atchops_rsa_decrypt(
      &privkey, ciphertext, sizeof(ciphertext), decrypted, sizeof(decrypted), &decrypted_len));

  TEST_ASSERT_EQUAL_size_t(plaintext_len, decrypted_len);
  TEST_ASSERT_EQUAL_STRING(plaintext, (char *)decrypted);

  atchops_rsa_key_public_key_free(&pubkey);
  atchops_rsa_key_private_key_free(&privkey);
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_rsa_sign_verify_roundtrip);
  RUN_TEST(test_rsa_verify_tampered_message_fails);
  RUN_TEST(test_rsa_encrypt_decrypt_roundtrip);
  return UNITY_END();
}
