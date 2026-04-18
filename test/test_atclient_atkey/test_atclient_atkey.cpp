/**
 * test_atclient_atkey.cpp
 *
 * Unity tests for atclient_atkey — key creation, field validation, and
 * string serialisation.  No network or crypto required; these are pure
 * string-manipulation tests that run identically on host and on-device.
 *
 * Mirrors the numbered-step pattern from at_c functional tests.
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atclient/atkey.h"
}

void setUp() {}
void tearDown() {}

/* ── Self key ────────────────────────────────────────────────────────────────
 *  Expected string:  "name.namespace@alice"
 */

void test_selfkey_create_fields() {
  atclient_atkey key;
  atclient_atkey_init(&key);

  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_self_key(&key, "name", "@alice", "namespace"));

  TEST_ASSERT_EQUAL_STRING("name",      key.key);
  TEST_ASSERT_EQUAL_STRING("namespace", key.namespace_str);
  TEST_ASSERT_EQUAL_STRING("@alice",    key.shared_by);
  TEST_ASSERT_NULL(key.shared_with);
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_SELF_KEY, atclient_atkey_get_type(&key));

  atclient_atkey_free(&key);
}

void test_selfkey_to_string() {
  atclient_atkey key;
  atclient_atkey_init(&key);
  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_self_key(&key, "greeting", "@alice", "myapp"));

  char *str = NULL;
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_to_string(&key, &str));
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("greeting.myapp@alice", str);

  free(str);
  atclient_atkey_free(&key);
}

void test_selfkey_to_string_no_namespace() {
  atclient_atkey key;
  atclient_atkey_init(&key);
  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_self_key(&key, "phone", "@alice", NULL));

  char *str = NULL;
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_to_string(&key, &str));
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("phone@alice", str);

  free(str);
  atclient_atkey_free(&key);
}

/* ── Public key ───────────────────────────────────────────────────────────────
 *  Expected string:  "public:name.namespace@alice"
 */

void test_publickey_create_fields() {
  atclient_atkey key;
  atclient_atkey_init(&key);

  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_public_key(&key, "publicinfo", "@alice", "myapp"));

  TEST_ASSERT_EQUAL_STRING("publicinfo", key.key);
  TEST_ASSERT_EQUAL_STRING("myapp",      key.namespace_str);
  TEST_ASSERT_EQUAL_STRING("@alice",     key.shared_by);
  TEST_ASSERT_NULL(key.shared_with);
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_PUBLIC_KEY, atclient_atkey_get_type(&key));

  atclient_atkey_free(&key);
}

void test_publickey_to_string() {
  atclient_atkey key;
  atclient_atkey_init(&key);
  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_public_key(&key, "location", "@alice", "myapp"));

  char *str = NULL;
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_to_string(&key, &str));
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("public:location.myapp@alice", str);

  free(str);
  atclient_atkey_free(&key);
}

/* ── Shared key ───────────────────────────────────────────────────────────────
 *  Expected string:  "@bob:name.namespace@alice"
 */

void test_sharedkey_create_fields() {
  atclient_atkey key;
  atclient_atkey_init(&key);

  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_shared_key(&key, "secret", "@alice", "@bob", "myapp"));

  TEST_ASSERT_EQUAL_STRING("secret",  key.key);
  TEST_ASSERT_EQUAL_STRING("myapp",   key.namespace_str);
  TEST_ASSERT_EQUAL_STRING("@alice",  key.shared_by);
  TEST_ASSERT_EQUAL_STRING("@bob",    key.shared_with);
  TEST_ASSERT_EQUAL_INT(ATCLIENT_ATKEY_TYPE_SHARED_KEY, atclient_atkey_get_type(&key));

  atclient_atkey_free(&key);
}

void test_sharedkey_to_string() {
  atclient_atkey key;
  atclient_atkey_init(&key);
  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_shared_key(&key, "message", "@alice", "@bob", "chat"));

  char *str = NULL;
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_to_string(&key, &str));
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("@bob:message.chat@alice", str);

  free(str);
  atclient_atkey_free(&key);
}

/* ── Clone ────────────────────────────────────────────────────────────────────
 *  The clone must be independent (freeing the original must not corrupt it).
 */

void test_atkey_clone() {
  atclient_atkey src, dst;
  atclient_atkey_init(&src);
  atclient_atkey_init(&dst);

  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_self_key(&src, "sample", "@charlie", "app"));
  TEST_ASSERT_EQUAL_INT(0, atclient_atkey_clone(&dst, &src));

  atclient_atkey_free(&src);  // freeing src must not corrupt dst

  TEST_ASSERT_EQUAL_STRING("sample",   dst.key);
  TEST_ASSERT_EQUAL_STRING("app",      dst.namespace_str);
  TEST_ASSERT_EQUAL_STRING("@charlie", dst.shared_by);

  atclient_atkey_free(&dst);
}

/* ── Double-free safety ───────────────────────────────────────────────────────
 *  Calling free on an already-freed (re-initialised) struct must not crash.
 */

void test_atkey_double_free_safe() {
  atclient_atkey key;
  atclient_atkey_init(&key);
  TEST_ASSERT_EQUAL_INT(0,
      atclient_atkey_create_self_key(&key, "tmp", "@dave", NULL));
  atclient_atkey_free(&key);
  atclient_atkey_init(&key);  // re-init before second free
  atclient_atkey_free(&key);  // must not crash
}

/* ── main ───────────────────────────────────────────────────────────────────── */

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_selfkey_create_fields);
  RUN_TEST(test_selfkey_to_string);
  RUN_TEST(test_selfkey_to_string_no_namespace);
  RUN_TEST(test_publickey_create_fields);
  RUN_TEST(test_publickey_to_string);
  RUN_TEST(test_sharedkey_create_fields);
  RUN_TEST(test_sharedkey_to_string);
  RUN_TEST(test_atkey_clone);
  RUN_TEST(test_atkey_double_free_safe);
  return UNITY_END();
}
