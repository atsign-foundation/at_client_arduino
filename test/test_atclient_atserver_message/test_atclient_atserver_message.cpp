/**
 * test_atclient_atserver_message.cpp
 *
 * Unity tests for atserver_message_parse.
 * Test cases taken directly from at_c/packages/atclient/tests/test_atserver_message.c
 */

#include <unity.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "atserver_message.h"
}

void setUp() {}
void tearDown() {}

/* ── 1a. long prompt: @foobar@data:baz\n ───────────────────────────────── */
void test_parse_long_prompt() {
  const uint16_t len = 17;
  char buffer[17];
  memcpy(buffer, "@foobar@data:baz\n", len);

  struct atserver_message msg = atserver_message_parse(buffer, len);

  TEST_ASSERT_NOT_NULL(msg.buffer);
  TEST_ASSERT_EQUAL_INT(8, (int)msg.prompt_len);
  TEST_ASSERT_EQUAL_INT(5, (int)msg.token_len);
  TEST_ASSERT_EQUAL_INT(3, (int)atserver_message_get_body_len(msg));
  TEST_ASSERT_EQUAL_INT(16, (int)msg.len);
}

/* ── 1b. short prompt: @data:baz\n ─────────────────────────────────────── */
void test_parse_short_prompt() {
  const uint16_t len = 10;
  char buffer[10];
  memcpy(buffer, "@data:baz\n", len);

  struct atserver_message msg = atserver_message_parse(buffer, len);

  TEST_ASSERT_NOT_NULL(msg.buffer);
  TEST_ASSERT_EQUAL_INT(1, (int)msg.prompt_len);
  TEST_ASSERT_EQUAL_INT(5, (int)msg.token_len);
  TEST_ASSERT_EQUAL_INT(3, (int)atserver_message_get_body_len(msg));
  TEST_ASSERT_EQUAL_INT(9, (int)msg.len);
}

/* ── 1c. no prompt: data:baz\n ──────────────────────────────────────────── */
void test_parse_no_prompt() {
  const uint16_t len = 9;
  char buffer[9];
  memcpy(buffer, "data:baz\n", len);

  struct atserver_message msg = atserver_message_parse(buffer, len);

  TEST_ASSERT_NOT_NULL(msg.buffer);
  TEST_ASSERT_EQUAL_INT(0, (int)msg.prompt_len);
  TEST_ASSERT_EQUAL_INT(5, (int)msg.token_len);
  TEST_ASSERT_EQUAL_INT(3, (int)atserver_message_get_body_len(msg));
  TEST_ASSERT_EQUAL_INT(8, (int)msg.len);
}

/* ── 2a. no token (invalid): @foo@baz\n → buffer should be NULL ─────────── */
void test_parse_no_token_returns_null() {
  const uint16_t len = 9;
  char buffer[9];
  memcpy(buffer, "@foo@baz\n", len);

  struct atserver_message msg = atserver_message_parse(buffer, len);

  TEST_ASSERT_NULL(msg.buffer);
  TEST_ASSERT_EQUAL_INT(0, (int)msg.prompt_len);
  TEST_ASSERT_EQUAL_INT(0, (int)msg.token_len);
  TEST_ASSERT_EQUAL_INT(0, (int)atserver_message_get_body_len(msg));
}

/* ── 3a. no body: @foobar@data:\n ──────────────────────────────────────── */
void test_parse_no_body() {
  const uint16_t len = 14;
  char buffer[14];
  memcpy(buffer, "@foobar@data:\n", len);

  struct atserver_message msg = atserver_message_parse(buffer, len);

  TEST_ASSERT_NOT_NULL(msg.buffer);
  TEST_ASSERT_EQUAL_INT(8, (int)msg.prompt_len);
  TEST_ASSERT_EQUAL_INT(5, (int)msg.token_len);
  TEST_ASSERT_EQUAL_INT(0, (int)atserver_message_get_body_len(msg));
  TEST_ASSERT_EQUAL_INT(13, (int)msg.len);
}

/* ── 4a. empty message: "" ──────────────────────────────────────────────── */
void test_parse_empty_message_returns_null() {
  char *buffer = (char *)"";

  struct atserver_message msg = atserver_message_parse(buffer, 0);

  TEST_ASSERT_NULL(msg.buffer);
  TEST_ASSERT_EQUAL_INT(0, (int)msg.len);
}

/* ── 5a. heap-allocated buffer: parse + mutate + free ───────────────────── */
void test_parse_heap_buffer() {
  const char *src = "@foobar@data:baz\n";
  const uint16_t len = (uint16_t)strlen(src);
  char *heap = (char *)malloc(len);
  TEST_ASSERT_NOT_NULL(heap);
  memcpy(heap, src, len);

  struct atserver_message msg = atserver_message_parse(heap, len);
  TEST_ASSERT_NOT_NULL(msg.buffer);
  TEST_ASSERT_EQUAL_INT(8, (int)msg.prompt_len);
  TEST_ASSERT_EQUAL_INT(5, (int)msg.token_len);
  TEST_ASSERT_EQUAL_INT(3, (int)atserver_message_get_body_len(msg));
  TEST_ASSERT_EQUAL_INT(16, (int)msg.len);

  /* message.buffer points into heap — mutate through it */
  msg.buffer[0] = 'H';
  TEST_ASSERT_EQUAL_INT(0, strncmp(heap, "Hfoobar@data:baz", len));

  atserver_message_free(&msg);
  TEST_ASSERT_NULL(msg.buffer);
  /* call again — double-free must not crash */
  atserver_message_free(&msg);
  /* heap is now owned and freed by atserver_message_free — do not call free(heap) */
}

/* ── 5b. bad parse on heap: original buffer must remain intact ──────────── */
void test_parse_heap_bad_parse_leaves_buffer_intact() {
  const char *src = "@foobar@baz\n";
  const uint16_t len = (uint16_t)strlen(src);
  char *heap = (char *)malloc(len);
  TEST_ASSERT_NOT_NULL(heap);
  memcpy(heap, src, len);

  struct atserver_message msg = atserver_message_parse(heap, len);
  TEST_ASSERT_NULL(msg.buffer);

  /* heap must not have been modified */
  TEST_ASSERT_EQUAL_INT(0, strncmp(heap, src, len - 1));

  free(heap);
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_long_prompt);
  RUN_TEST(test_parse_short_prompt);
  RUN_TEST(test_parse_no_prompt);
  RUN_TEST(test_parse_no_token_returns_null);
  RUN_TEST(test_parse_no_body);
  RUN_TEST(test_parse_empty_message_returns_null);
  RUN_TEST(test_parse_heap_buffer);
  RUN_TEST(test_parse_heap_bad_parse_leaves_buffer_intact);
  return UNITY_END();
}
