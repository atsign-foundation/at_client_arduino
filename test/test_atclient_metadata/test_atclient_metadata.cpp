/**
 * test_atclient_metadata.cpp
 *
 * Unity tests for atclient_atkey_metadata JSON parsing and protocol string
 * serialisation. Test data taken from at_c/packages/atclient/tests/test_atkey_metadata.c
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

extern "C" {
#include "atclient/metadata.h"
}

void setUp() {}
void tearDown() {}

/* ── parse a real metadata JSON blob (from a qt_thermostat atServer response) */
static const char *METADATA_JSON =
    "{"
    "\"createdBy\":\"@qt_thermostat\","
    "\"updatedBy\":\"@qt_thermostat\","
    "\"createdAt\":\"2024-02-17 19:54:12.037Z\","
    "\"updatedAt\":\"2024-02-17 19:54:12.037Z\","
    "\"expiresAt\":\"2024-02-17 19:55:38.437Z\","
    "\"status\":\"active\","
    "\"version\":0,"
    "\"ttl\":86400,"
    "\"isBinary\":false,"
    "\"isEncrypted\":false"
    "}";

void test_metadata_from_json_str() {
  atclient_atkey_metadata metadata;
  atclient_atkey_metadata_init(&metadata);

  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_metadata_from_json_str(&metadata, METADATA_JSON));

  TEST_ASSERT_TRUE(atclient_atkey_metadata_is_created_by_initialized(&metadata));
  TEST_ASSERT_EQUAL_STRING("@qt_thermostat", metadata.created_by);

  TEST_ASSERT_TRUE(atclient_atkey_metadata_is_updated_by_initialized(&metadata));
  TEST_ASSERT_EQUAL_STRING("@qt_thermostat", metadata.updated_by);

  TEST_ASSERT_TRUE(atclient_atkey_metadata_is_created_at_initialized(&metadata));
  TEST_ASSERT_EQUAL_STRING("2024-02-17 19:54:12.037Z", metadata.created_at);

  TEST_ASSERT_TRUE(atclient_atkey_metadata_is_updated_at_initialized(&metadata));
  TEST_ASSERT_EQUAL_STRING("2024-02-17 19:54:12.037Z", metadata.updated_at);

  TEST_ASSERT_TRUE(atclient_atkey_metadata_is_expires_at_initialized(&metadata));
  TEST_ASSERT_EQUAL_STRING("2024-02-17 19:55:38.437Z", metadata.expires_at);

  TEST_ASSERT_TRUE(atclient_atkey_metadata_is_ttl_initialized(&metadata));
  TEST_ASSERT_EQUAL_INT(86400, (int)metadata.ttl);

  TEST_ASSERT_TRUE(atclient_atkey_metadata_is_is_binary_initialized(&metadata));
  TEST_ASSERT_FALSE(metadata.is_binary);

  TEST_ASSERT_TRUE(atclient_atkey_metadata_is_is_encrypted_initialized(&metadata));
  TEST_ASSERT_FALSE(metadata.is_encrypted);

  /* Fields not in JSON should NOT be initialized */
  TEST_ASSERT_FALSE(atclient_atkey_metadata_is_is_cached_initialized(&metadata));
  TEST_ASSERT_FALSE(atclient_atkey_metadata_is_available_at_initialized(&metadata));

  atclient_atkey_metadata_free(&metadata);
}

/* ── round-trip: parse JSON then write it back */
void test_metadata_roundtrip_json() {
  atclient_atkey_metadata metadata;
  atclient_atkey_metadata_init(&metadata);

  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_metadata_from_json_str(&metadata, METADATA_JSON));

  char *jsonstr = NULL;
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_metadata_to_json_str(&metadata, &jsonstr));
  TEST_ASSERT_NOT_NULL(jsonstr);

  free(jsonstr);
  atclient_atkey_metadata_free(&metadata);
}

/* ── protocol string: :ttr:-1:isBinary:true:isEncrypted:true:ivNonce:abcdefghijk */
void test_metadata_to_protocol_str() {
  const char *expected = ":ttr:-1:isBinary:true:isEncrypted:true:ivNonce:abcdefghijk";

  atclient_atkey_metadata metadata;
  atclient_atkey_metadata_init(&metadata);

  atclient_atkey_metadata_set_ttr(&metadata, -1);
  atclient_atkey_metadata_set_is_binary(&metadata, true);
  atclient_atkey_metadata_set_is_encrypted(&metadata, true);
  atclient_atkey_metadata_set_is_cached(&metadata, true);
  atclient_atkey_metadata_set_iv_nonce(&metadata, "abcdefghijk");

  char *protocolfragment = NULL;
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_metadata_to_protocol_str(&metadata, &protocolfragment));
  TEST_ASSERT_NOT_NULL(protocolfragment);
  TEST_ASSERT_EQUAL_STRING(expected, protocolfragment);

  free(protocolfragment);
  atclient_atkey_metadata_free(&metadata);
}

/* ── noports device-info key metadata: :ttl:2592000000:ttr:-1:ccd:true:isEncrypted:true:ivNonce:abcdefghijk */
void test_metadata_to_protocol_str_noports() {
  const char *expected = ":ttl:2592000000:ttr:-1:ccd:true:isEncrypted:true:ivNonce:abcdefghijk";

  atclient_atkey_metadata metadata;
  atclient_atkey_metadata_init(&metadata);

  atclient_atkey_metadata_set_is_public(&metadata, false);
  atclient_atkey_metadata_set_is_encrypted(&metadata, true);
  atclient_atkey_metadata_set_ttr(&metadata, -1);
  atclient_atkey_metadata_set_ccd(&metadata, true);
  atclient_atkey_metadata_set_ttl(&metadata, (long)30 * 24 * 60 * 60 * 1000); /* 30 days in ms */
  atclient_atkey_metadata_set_iv_nonce(&metadata, "abcdefghijk");

  char *protocolfragment = NULL;
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_metadata_to_protocol_str(&metadata, &protocolfragment));
  TEST_ASSERT_NOT_NULL(protocolfragment);
  TEST_ASSERT_EQUAL_STRING(expected, protocolfragment);

  free(protocolfragment);
  atclient_atkey_metadata_free(&metadata);
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_metadata_from_json_str);
  RUN_TEST(test_metadata_roundtrip_json);
  RUN_TEST(test_metadata_to_protocol_str);
  RUN_TEST(test_metadata_to_protocol_str_noports);
  return UNITY_END();
}
