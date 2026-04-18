/**
 * test_atclient_atkey_from_string.cpp
 *
 * Unity tests for atclient_atkey_from_string covering all key types.
 * Test strings taken directly from at_c/packages/atclient/tests/test_atkey_from_string.c
 */

#include <unity.h>
#include <string.h>
#include <stdbool.h>

extern "C" {
#include "atclient/atkey.h"
#include "atclient/metadata.h"
}

void setUp() {}
void tearDown() {}

/* ── 1. Public keys ──────────────────────────────────────────────────────── */

void test_from_string_cached_public_no_namespace() {
  /* cached:public:publickey@bob */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "cached:public:publickey@bob"));
  TEST_ASSERT_TRUE(atkey.metadata.is_cached);
  TEST_ASSERT_TRUE(atkey.metadata.is_public);
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_PUBLIC_KEY, (int)atclient_atkey_get_type(&atkey));
  TEST_ASSERT_EQUAL_STRING("publickey", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@bob", atkey.shared_by);
  atclient_atkey_free(&atkey);
}

void test_from_string_public_no_namespace() {
  /* public:publickey@alice */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "public:publickey@alice"));
  TEST_ASSERT_TRUE(atkey.metadata.is_public);
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_PUBLIC_KEY, (int)atclient_atkey_get_type(&atkey));
  TEST_ASSERT_EQUAL_STRING("publickey", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@alice", atkey.shared_by);
  TEST_ASSERT_FALSE(atclient_atkey_is_namespacestr_initialized(&atkey));
  TEST_ASSERT_FALSE(atclient_atkey_is_shared_with_initialized(&atkey));
  atclient_atkey_free(&atkey);
}

void test_from_string_public_with_namespace() {
  /* public:name.wavi@jeremy */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "public:name.wavi@jeremy"));
  TEST_ASSERT_TRUE(atkey.metadata.is_public);
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_PUBLIC_KEY, (int)atclient_atkey_get_type(&atkey));
  TEST_ASSERT_EQUAL_STRING("name", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@jeremy", atkey.shared_by);
  TEST_ASSERT_EQUAL_STRING("wavi", atkey.namespace_str);
  atclient_atkey_free(&atkey);
}

/* ── 2. Shared keys ──────────────────────────────────────────────────────── */

void test_from_string_shared_with_namespace() {
  /* @alice:name.wavi@bob */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "@alice:name.wavi@bob"));
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_SHARED_KEY, (int)atclient_atkey_get_type(&atkey));
  TEST_ASSERT_EQUAL_STRING("name", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@bob", atkey.shared_by);
  TEST_ASSERT_EQUAL_STRING("@alice", atkey.shared_with);
  TEST_ASSERT_EQUAL_STRING("wavi", atkey.namespace_str);
  atclient_atkey_free(&atkey);
}

void test_from_string_cached_shared_no_namespace() {
  /* cached:@bob:name@alice */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "cached:@bob:name@alice"));
  TEST_ASSERT_TRUE(atkey.metadata.is_cached);
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_SHARED_KEY, (int)atclient_atkey_get_type(&atkey));
  TEST_ASSERT_EQUAL_STRING("name", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@alice", atkey.shared_by);
  TEST_ASSERT_EQUAL_STRING("@bob", atkey.shared_with);
  TEST_ASSERT_FALSE(atclient_atkey_is_namespacestr_initialized(&atkey));
  atclient_atkey_free(&atkey);
}

void test_from_string_shared_no_namespace() {
  /* @bob:name@alice */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "@bob:name@alice"));
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_SHARED_KEY, (int)atclient_atkey_get_type(&atkey));
  TEST_ASSERT_EQUAL_STRING("name", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@alice", atkey.shared_by);
  TEST_ASSERT_EQUAL_STRING("@bob", atkey.shared_with);
  TEST_ASSERT_FALSE(atclient_atkey_is_namespacestr_initialized(&atkey));
  atclient_atkey_free(&atkey);
}

void test_from_string_shared_compounding_namespace() {
  /* @alice:name.vpsx.sshnp.abcd.efgh@xavierbob123 */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "@alice:name.vpsx.sshnp.abcd.efgh@xavierbob123"));
  TEST_ASSERT_EQUAL_STRING("name", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@xavierbob123", atkey.shared_by);
  TEST_ASSERT_EQUAL_STRING("@alice", atkey.shared_with);
  TEST_ASSERT_EQUAL_STRING("vpsx.sshnp.abcd.efgh", atkey.namespace_str);
  atclient_atkey_free(&atkey);
}

/* ── 3. Self keys ────────────────────────────────────────────────────────── */

void test_from_string_self_no_namespace() {
  /* name@alice */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "name@alice"));
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_SELF_KEY, (int)atclient_atkey_get_type(&atkey));
  TEST_ASSERT_EQUAL_STRING("name", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@alice", atkey.shared_by);
  TEST_ASSERT_FALSE(atclient_atkey_is_shared_with_initialized(&atkey));
  TEST_ASSERT_FALSE(atclient_atkey_is_namespacestr_initialized(&atkey));
  atclient_atkey_free(&atkey);
}

void test_from_string_self_with_namespace() {
  /* name.wavi@jeremy_0 */
  atclient_atkey atkey;
  atclient_atkey_init(&atkey);
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_from_string(&atkey, "name.wavi@jeremy_0"));
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_SELF_KEY, (int)atclient_atkey_get_type(&atkey));
  TEST_ASSERT_EQUAL_STRING("name", atkey.key);
  TEST_ASSERT_EQUAL_STRING("@jeremy_0", atkey.shared_by);
  TEST_ASSERT_EQUAL_STRING("wavi", atkey.namespace_str);
  TEST_ASSERT_FALSE(atclient_atkey_is_shared_with_initialized(&atkey));
  atclient_atkey_free(&atkey);
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_from_string_cached_public_no_namespace);
  RUN_TEST(test_from_string_public_no_namespace);
  RUN_TEST(test_from_string_public_with_namespace);
  RUN_TEST(test_from_string_shared_with_namespace);
  RUN_TEST(test_from_string_cached_shared_no_namespace);
  RUN_TEST(test_from_string_shared_no_namespace);
  RUN_TEST(test_from_string_shared_compounding_namespace);
  RUN_TEST(test_from_string_self_no_namespace);
  RUN_TEST(test_from_string_self_with_namespace);
  return UNITY_END();
}
