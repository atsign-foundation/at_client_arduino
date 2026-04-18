#include "atclient/rpc.h"
#include "atclient/atkey.h"
#include "atclient/metadata.h"
#include "atclient/notify.h"
#include "atclient/notify_params.h"
#include "atlogger/atlogger.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "atclient_rpc"

// ---------------------------------------------------------------------------
// atclient_rpc_send_request
// ---------------------------------------------------------------------------

int atclient_rpc_send_request(atclient   *atc,
                               const char *from_atsign,
                               const char *to_atsign,
                               const char *base_ns,
                               const char *domain_ns,
                               uint32_t    req_id,
                               const char *payload_json,
                               uint32_t    ttl_ms) {
  int ret = -1;

  if (atc == NULL || from_atsign == NULL || to_atsign == NULL ||
      base_ns == NULL || domain_ns == NULL || payload_json == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                 "atclient_rpc_send_request: NULL argument\n");
    return -1;
  }

  // 1. Build the request key name:
  //    "request.<req_id>.<domain_ns>.<ATCLIENT_RPC_NS_RPCS>"
  char keyname[128];
  snprintf(keyname, sizeof(keyname), "request.%lu.%s.%s",
           (unsigned long)req_id, domain_ns, ATCLIENT_RPC_NS_RPCS);

  // 2. Create the shared atkey (sharedBy=from_atsign, sharedWith=to_atsign)
  atclient_atkey key;
  atclient_atkey_init(&key);

  ret = atclient_atkey_create_shared_key(&key, keyname, from_atsign,
                                          to_atsign, base_ns);
  if (ret != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                 "atclient_rpc_send_request: failed to create atkey (%d)\n", ret);
    atclient_atkey_free(&key);
    return ret;
  }

  // 3. Set metadata: encrypted, private, TTL
  atclient_atkey_metadata_set_is_public(&key.metadata, false);
  atclient_atkey_metadata_set_is_encrypted(&key.metadata, true);
  uint32_t effective_ttl = (ttl_ms > 0) ? ttl_ms : ATCLIENT_RPC_DEFAULT_TTL_MS;
  atclient_atkey_metadata_set_ttl(&key.metadata, (int64_t)effective_ttl);

  // 4. Build the AtRpcReq JSON envelope:
  //    {"reqId":<req_id>,"payload":<payload_json>}
  //    payload_json is a full JSON object string, e.g. {"foo":"bar"}
  size_t value_size = 16 + 20 + strlen(payload_json) + 4;
  char *value = (char *)malloc(value_size);
  if (value == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                 "atclient_rpc_send_request: OOM building value\n");
    atclient_atkey_free(&key);
    return -1;
  }
  snprintf(value, value_size, "{\"reqId\":%lu,\"payload\":%s}",
           (unsigned long)req_id, payload_json);

  // 5. Build notify params and send
  atclient_notify_params params;
  atclient_notify_params_init(&params);
  atclient_notify_params_set_atkey(&params, &key);
  atclient_notify_params_set_operation(&params, ATCLIENT_NOTIFY_OPERATION_UPDATE);
  atclient_notify_params_set_value(&params, value);

  ret = atclient_notify(atc, &params, NULL);
  if (ret != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "atclient_rpc_send_request: atclient_notify failed (%d)\n", ret);
  }

  free(value);
  atclient_notify_params_free(&params);
  atclient_atkey_free(&key);
  return ret;
}

// ---------------------------------------------------------------------------
// atclient_rpc_is_response_key
// ---------------------------------------------------------------------------

bool atclient_rpc_is_response_key(const char *key,
                                   const char *base_ns,
                                   const char *domain_ns) {
  if (key == NULL || base_ns == NULL || domain_ns == NULL) {
    return false;
  }

  // The key must contain the RPC namespace pattern.
  // Two possible forms depending on whether the sender used namespaceAware=true
  // (C SDK default, namespace appended) or namespaceAware=false (Dart AtRpc
  // default):
  //
  //   namespaceAware=true  → "…<domain_ns>.<rpcs_ns>.<base_ns>@…"
  //   namespaceAware=false → "…<domain_ns>.<rpcs_ns>@…"   (no base_ns)
  //
  // Accept either form so we interoperate with the Dart AtRpc library.
  char suffix_ns[192];
  snprintf(suffix_ns, sizeof(suffix_ns), "%s.%s.%s@",
           domain_ns, ATCLIENT_RPC_NS_RPCS, base_ns);
  char suffix_no_ns[128];
  snprintf(suffix_no_ns, sizeof(suffix_no_ns), "%s.%s@",
           domain_ns, ATCLIENT_RPC_NS_RPCS);
  if (strstr(key, suffix_ns) == NULL && strstr(key, suffix_no_ns) == NULL) {
    return false;
  }

  // Strip any "@atsign:" prefix so we are left with the key body:
  //   "<resp_type>.<req_id>.<domain_ns>.<rpcs_ns>.<base_ns>@<from>"
  const char *body = key;
  const char *colon = strchr(key, ':');
  if (colon != NULL) {
    body = colon + 1;
  }

  return (strncmp(body, "ack.",      4) == 0 ||
          strncmp(body, "nack.",     5) == 0 ||
          strncmp(body, "success.",  8) == 0 ||
          strncmp(body, "error.",    6) == 0);
}

// ---------------------------------------------------------------------------
// atclient_rpc_parse_response_key
// ---------------------------------------------------------------------------

int atclient_rpc_parse_response_key(const atclient_atnotification *notification,
                                     atclient_rpc_resp             *resp) {
  if (notification == NULL || notification->key == NULL || resp == NULL) {
    return -1;
  }

  resp->req_id    = 0;
  resp->resp_type = ATCLIENT_RPC_RESP_NONE;

  // Strip the "@atsign:" prefix that the monitor sometimes prepends.
  const char *body = notification->key;
  const char *colon = strchr(body, ':');
  if (colon != NULL) {
    body = colon + 1;
  }

  // body is now: "<resp_type>.<req_id>.<domain_ns>.<rpcs_ns>.<base_ns>@<from>"

  // --- Parse resp_type (up to first '.') ---
  const char *dot1 = strchr(body, '.');
  if (dot1 == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_response_key: no '.' in key body: %s\n", body);
    return -1;
  }
  size_t rt_len = (size_t)(dot1 - body);
  char resp_type_str[16];
  if (rt_len == 0 || rt_len >= sizeof(resp_type_str)) {
    return -1;
  }
  memcpy(resp_type_str, body, rt_len);
  resp_type_str[rt_len] = '\0';

  if      (strcmp(resp_type_str, "ack")     == 0) resp->resp_type = ATCLIENT_RPC_RESP_ACK;
  else if (strcmp(resp_type_str, "nack")    == 0) resp->resp_type = ATCLIENT_RPC_RESP_NACK;
  else if (strcmp(resp_type_str, "success") == 0) resp->resp_type = ATCLIENT_RPC_RESP_SUCCESS;
  else if (strcmp(resp_type_str, "error")   == 0) resp->resp_type = ATCLIENT_RPC_RESP_ERROR;
  else {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_response_key: unknown resp_type '%s'\n", resp_type_str);
    return -1;
  }

  // --- Parse req_id (between first '.' and second '.') ---
  const char *dot2 = strchr(dot1 + 1, '.');
  if (dot2 == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_response_key: no second '.' in key body: %s\n", body);
    return -1;
  }
  size_t id_len = (size_t)(dot2 - (dot1 + 1));
  char req_id_str[20];
  if (id_len == 0 || id_len >= sizeof(req_id_str)) {
    return -1;
  }
  memcpy(req_id_str, dot1 + 1, id_len);
  req_id_str[id_len] = '\0';

  char *end = NULL;
  unsigned long parsed = strtoul(req_id_str, &end, 10);
  if (end == req_id_str || (end != NULL && *end != '\0')) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_response_key: invalid req_id '%s'\n", req_id_str);
    return -1;
  }

  resp->req_id = (uint32_t)parsed;
  return 0;
}

// ---------------------------------------------------------------------------
// atclient_rpc_is_request_key
// ---------------------------------------------------------------------------

bool atclient_rpc_is_request_key(const char *key,
                                  const char *base_ns,
                                  const char *domain_ns) {
  if (key == NULL || base_ns == NULL || domain_ns == NULL) {
    return false;
  }

  // Must contain the RPC namespace pattern — accept both with and without
  // base_ns suffix (namespaceAware=true vs namespaceAware=false).
  char suffix_ns[192];
  snprintf(suffix_ns, sizeof(suffix_ns), "%s.%s.%s@",
           domain_ns, ATCLIENT_RPC_NS_RPCS, base_ns);
  char suffix_no_ns[128];
  snprintf(suffix_no_ns, sizeof(suffix_no_ns), "%s.%s@",
           domain_ns, ATCLIENT_RPC_NS_RPCS);
  if (strstr(key, suffix_ns) == NULL && strstr(key, suffix_no_ns) == NULL) {
    return false;
  }

  // Strip any "@atsign:" prefix
  const char *body = key;
  const char *colon = strchr(key, ':');
  if (colon != NULL) {
    body = colon + 1;
  }

  return strncmp(body, "request.", 8) == 0;
}

// ---------------------------------------------------------------------------
// atclient_rpc_parse_request
// ---------------------------------------------------------------------------

int atclient_rpc_parse_request(const atclient_atnotification *notification,
                                atclient_rpc_req              *req) {
  if (notification == NULL || notification->key == NULL || req == NULL) {
    return -1;
  }

  req->req_id       = 0;
  req->payload_json = NULL;

  // --- Parse req_id from the key ---
  // Key body: "request.<req_id>.<domain_ns>.<rpcs_ns>.<base_ns>@<from>"
  const char *body = notification->key;
  const char *colon = strchr(body, ':');
  if (colon != NULL) {
    body = colon + 1;
  }

  // Skip "request."
  if (strncmp(body, "request.", 8) != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_request: key does not start with 'request.': %s\n", body);
    return -1;
  }
  const char *id_start = body + 8;
  const char *dot = strchr(id_start, '.');
  if (dot == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_request: malformed key: %s\n", body);
    return -1;
  }
  size_t id_len = (size_t)(dot - id_start);
  char req_id_str[20];
  if (id_len == 0 || id_len >= sizeof(req_id_str)) {
    return -1;
  }
  memcpy(req_id_str, id_start, id_len);
  req_id_str[id_len] = '\0';

  char *end = NULL;
  unsigned long parsed = strtoul(req_id_str, &end, 10);
  if (end == req_id_str || (end != NULL && *end != '\0')) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_request: invalid req_id '%s'\n", req_id_str);
    return -1;
  }
  req->req_id = (uint32_t)parsed;

  // --- Locate payload_json in decrypted_value ---
  // The envelope is: {"reqId":<n>,"payload":{...}}
  // We need to point at the value of "payload".
  if (notification->decrypted_value == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_request: no decrypted_value\n");
    return -1;
  }

  // Find `"payload":` then advance past it to the value start
  const char *pl_key = strstr(notification->decrypted_value, "\"payload\":");
  if (pl_key == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "parse_request: no 'payload' field in envelope\n");
    return -1;
  }
  const char *pl_val = pl_key + strlen("\"payload\":");
  // Skip any whitespace
  while (*pl_val == ' ' || *pl_val == '\t' || *pl_val == '\n') pl_val++;
  if (*pl_val == '\0') {
    return -1;
  }

  req->payload_json = pl_val;  // points into decrypted_value — no copy
  return 0;
}

// ---------------------------------------------------------------------------
// atclient_rpc_send_response
// ---------------------------------------------------------------------------

int atclient_rpc_send_response(atclient              *atc,
                                const char            *from_atsign,
                                const char            *to_atsign,
                                const char            *base_ns,
                                const char            *domain_ns,
                                uint32_t               req_id,
                                atclient_rpc_resp_type resp_type,
                                const char            *payload_json,
                                const char            *message,
                                uint32_t               ttl_ms) {
  int ret = -1;

  if (atc == NULL || from_atsign == NULL || to_atsign == NULL ||
      base_ns == NULL || domain_ns == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                 "atclient_rpc_send_response: NULL argument\n");
    return -1;
  }

  // Map enum to wire string
  const char *resp_type_str;
  switch (resp_type) {
    case ATCLIENT_RPC_RESP_ACK:     resp_type_str = "ack";     break;
    case ATCLIENT_RPC_RESP_NACK:    resp_type_str = "nack";    break;
    case ATCLIENT_RPC_RESP_SUCCESS: resp_type_str = "success"; break;
    case ATCLIENT_RPC_RESP_ERROR:   resp_type_str = "error";   break;
    default:
      atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                   "atclient_rpc_send_response: unknown resp_type %d\n", (int)resp_type);
      return -1;
  }

  // Build key: "<resp_type_str>.<req_id>.<domain_ns>.<ATCLIENT_RPC_NS_RPCS>"
  char keyname[128];
  snprintf(keyname, sizeof(keyname), "%s.%lu.%s.%s",
           resp_type_str, (unsigned long)req_id, domain_ns, ATCLIENT_RPC_NS_RPCS);

  atclient_atkey key;
  atclient_atkey_init(&key);

  ret = atclient_atkey_create_shared_key(&key, keyname, from_atsign,
                                          to_atsign, base_ns);
  if (ret != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                 "atclient_rpc_send_response: failed to create atkey (%d)\n", ret);
    atclient_atkey_free(&key);
    return ret;
  }

  atclient_atkey_metadata_set_is_public(&key.metadata, false);
  atclient_atkey_metadata_set_is_encrypted(&key.metadata, true);
  uint32_t effective_ttl = (ttl_ms > 0) ? ttl_ms : ATCLIENT_RPC_DEFAULT_TTL_MS;
  atclient_atkey_metadata_set_ttl(&key.metadata, (int64_t)effective_ttl);

  // Build AtRpcResp JSON:
  //   {"reqId":<n>,"respType":"<type>","payload":<payload_json>,"message":<message>}
  const char *pl  = (payload_json && *payload_json) ? payload_json : "{}";
  const char *msg = message ? message : "";

  // Rough size: fixed fields + payload + message
  size_t value_size = 64 + 20 + strlen(pl) + strlen(msg) + 8;
  char *value = (char *)malloc(value_size);
  if (value == NULL) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_ERROR,
                 "atclient_rpc_send_response: OOM building value\n");
    atclient_atkey_free(&key);
    return -1;
  }
  snprintf(value, value_size,
           "{\"reqId\":%lu,\"respType\":\"%s\",\"payload\":%s,\"message\":\"%s\"}",
           (unsigned long)req_id, resp_type_str, pl, msg);

  atclient_notify_params params;
  atclient_notify_params_init(&params);
  atclient_notify_params_set_atkey(&params, &key);
  atclient_notify_params_set_operation(&params, ATCLIENT_NOTIFY_OPERATION_UPDATE);
  atclient_notify_params_set_value(&params, value);

  ret = atclient_notify(atc, &params, NULL);
  if (ret != 0) {
    atlogger_log(TAG, ATLOGGER_LOGGING_LEVEL_WARN,
                 "atclient_rpc_send_response: atclient_notify failed (%d)\n", ret);
  }

  free(value);
  atclient_notify_params_free(&params);
  atclient_atkey_free(&key);
  return ret;
}
