#ifndef ATCLIENT_RPC_H
#define ATCLIENT_RPC_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file rpc.h
 * @brief A simple RPC request/response abstraction over atProtocol notifications.
 *
 * Wire format mirrors the Dart at_client AtRpc implementation:
 *   Request key:  request.<reqId>.<domainNS>.<ATCLIENT_RPC_NS_RPCS>.<baseNS>
 *   Response key: <ack|nack|success|error>.<reqId>.<domainNS>.<ATCLIENT_RPC_NS_RPCS>.<baseNS>
 *
 * Request notification value (JSON):
 *   {"reqId":<uint32>,"payload":<app-defined JSON object string>}
 *
 * Response notification value (JSON):
 *   {"reqId":<uint32>,"respType":"<ack|nack|success|error>",
 *    "payload":{...},"message":"..."}
 *
 * CLIENT USAGE (sending a request, receiving a response):
 *   1. Call atclient_rpc_send_request() to send.
 *   2. In the monitor loop call atclient_rpc_is_response_key() for early-out.
 *   3. On match call atclient_rpc_parse_response_key() to get resp_type/req_id.
 *   4. Parse the JSON payload from notification->decrypted_value yourself.
 *
 * SERVER USAGE (receiving a request, sending a response):
 *   1. In the monitor loop call atclient_rpc_is_request_key() for early-out.
 *   2. On match call atclient_rpc_parse_request() to get req_id + payload pointer.
 *   3. Process the payload (notification->decrypted_value gives the full JSON).
 *   4. Call atclient_rpc_send_response() with ATCLIENT_RPC_RESP_ACK first,
 *      then again with SUCCESS/ERROR/NACK once processing is done.
 */

#include "atclient.h"
#include "atnotification.h"
#include "../atchops/platform.h" // IWYU pragma: keep
#include <stdbool.h>
#include <stdint.h>

/** The fixed RPC sub-namespace, matching Dart's AtRpc.rpcsNameSpace default. */
#define ATCLIENT_RPC_NS_RPCS "__rpcs"

/** Default TTL (ms) applied to request notifications when ttl_ms == 0. */
#define ATCLIENT_RPC_DEFAULT_TTL_MS 30000

/**
 * @brief Parsed request descriptor populated by atclient_rpc_parse_request().
 *
 * The full JSON envelope remains in notification->decrypted_value.
 * payload_json points INTO that buffer (no copy) and is only valid while
 * the notification is alive.
 */
typedef struct atclient_rpc_req {
  uint32_t    req_id;       /**< Unique request ID from the sender */
  const char *payload_json; /**< Points into decrypted_value — do NOT free */
} atclient_rpc_req;

/**
 * @brief Possible response types parsed from an RPC response notification key.
 */
typedef enum atclient_rpc_resp_type {
  ATCLIENT_RPC_RESP_NONE    = 0, /**< Not yet parsed / unknown */
  ATCLIENT_RPC_RESP_ACK     = 1, /**< Server received request; still processing */
  ATCLIENT_RPC_RESP_NACK    = 2, /**< Server rejected request (bad format, etc.) */
  ATCLIENT_RPC_RESP_SUCCESS = 3, /**< Request handled successfully */
  ATCLIENT_RPC_RESP_ERROR   = 4, /**< Request failed during handling */
} atclient_rpc_resp_type;

/**
 * @brief Minimal parsed response descriptor populated by
 *        atclient_rpc_parse_response_key().
 *
 * The full JSON payload remains in notification->decrypted_value and should
 * be parsed by the caller's application code.
 */
typedef struct atclient_rpc_resp {
  uint32_t               req_id;    /**< Correlated request ID */
  atclient_rpc_resp_type resp_type; /**< Response type */
} atclient_rpc_resp;

/**
 * @brief Send an RPC request notification.
 *
 * Builds the key `request.<req_id>.<domain_ns>.ATCLIENT_RPC_NS_RPCS` shared
 * from @from_atsign to @to_atsign under @base_ns, wraps @payload_json in the
 * standard AtRpcReq envelope, and sends it via atclient_notify().
 *
 * @param atc          Authenticated atclient instance
 * @param from_atsign  The sending (device) atSign
 * @param to_atsign    The target (service / policy) atSign
 * @param base_ns      Application namespace, e.g. "sshnp"
 * @param domain_ns    Domain namespace, e.g. "auth_checks"
 * @param req_id       Unique request ID (caller manages uniqueness)
 * @param payload_json JSON *object* string for the "payload" field.
 *                     Must be a valid JSON object like {"key":"value"}.
 * @param ttl_ms       Notification TTL in milliseconds.  Pass 0 for default.
 * @return 0 on success, non-zero on failure.
 */
int atclient_rpc_send_request(atclient   *atc,
                               const char *from_atsign,
                               const char *to_atsign,
                               const char *base_ns,
                               const char *domain_ns,
                               uint32_t    req_id,
                               const char *payload_json,
                               uint32_t    ttl_ms);

/**
 * @brief Test whether a notification key is an RPC response for a given
 *        base/domain namespace combination.
 *
 * Returns true if the key contains the pattern
 *   `<domain_ns>.ATCLIENT_RPC_NS_RPCS.<base_ns>@`
 * and its body (after stripping any `@atsign:` prefix) starts with one of
 * `ack.`, `nack.`, `success.`, or `error.`.
 *
 * Suitable for use in a monitor loop early-out check.
 *
 * @param key       Raw notification key (notification->key)
 * @param base_ns   Application namespace
 * @param domain_ns Domain namespace
 */
bool atclient_rpc_is_response_key(const char *key,
                                   const char *base_ns,
                                   const char *domain_ns);

/**
 * @brief Parse the response type and request ID from an RPC response
 *        notification key.
 *
 * Strips any `@atsign:` prefix from the key, then extracts resp_type
 * and req_id.
 *
 * The full JSON payload (authorized, permitOpen, message, etc.) is NOT
 * parsed here — it stays in notification->decrypted_value for the caller
 * to read with its own JSON library.
 *
 * @param notification  Notification from the monitor (key must be populated)
 * @param resp          Output: populated with req_id and resp_type on success
 * @return 0 on success, non-zero if the key could not be parsed.
 */
int atclient_rpc_parse_response_key(const atclient_atnotification *notification,
                                     atclient_rpc_resp             *resp);

// ============================================================================
// Server-side API
// ============================================================================

/**
 * @brief Test whether a notification key is an RPC request for a given
 *        base/domain namespace combination.
 *
 * Returns true if the key body (after stripping any `@atsign:` prefix)
 * starts with `request.` and contains the pattern
 *   `<domain_ns>.ATCLIENT_RPC_NS_RPCS.<base_ns>@`.
 *
 * @param key       Raw notification key (notification->key)
 * @param base_ns   Application namespace
 * @param domain_ns Domain namespace
 */
bool atclient_rpc_is_request_key(const char *key,
                                  const char *base_ns,
                                  const char *domain_ns);

/**
 * @brief Parse the request ID from an RPC request notification key and
 *        locate the payload JSON within the decrypted value.
 *
 * On success, req->req_id is set and req->payload_json points into
 * notification->decrypted_value at the start of the "payload" object.
 * The pointer is only valid while the notification is alive.
 *
 * @param notification  Notification from the monitor (key + decrypted_value
 *                      must be populated)
 * @param req           Output: populated on success
 * @return 0 on success, non-zero if parsing fails.
 */
int atclient_rpc_parse_request(const atclient_atnotification *notification,
                                atclient_rpc_req              *req);

/**
 * @brief Send an RPC response notification back to the original requester.
 *
 * Builds the key `<resp_type_str>.<req_id>.<domain_ns>.ATCLIENT_RPC_NS_RPCS`
 * shared from @from_atsign to @to_atsign under @base_ns, and sends it via
 * atclient_notify().
 *
 * For ACK responses, pass NULL for payload_json and message.
 * For SUCCESS/ERROR responses, pass a JSON object string for payload_json.
 *
 * @param atc           Authenticated atclient instance
 * @param from_atsign   The server's (responder's) atSign
 * @param to_atsign     The client's (requester's) atSign
 * @param base_ns       Application namespace, e.g. "sshnp"
 * @param domain_ns     Domain namespace, e.g. "auth_checks"
 * @param req_id        The req_id from the original request
 * @param resp_type     Response type (ACK / NACK / SUCCESS / ERROR)
 * @param payload_json  JSON object string for the "payload" field, or NULL.
 * @param message       Optional human-readable message string, or NULL.
 * @param ttl_ms        TTL in milliseconds; 0 uses ATCLIENT_RPC_DEFAULT_TTL_MS.
 * @return 0 on success, non-zero on failure.
 */
int atclient_rpc_send_response(atclient              *atc,
                                const char            *from_atsign,
                                const char            *to_atsign,
                                const char            *base_ns,
                                const char            *domain_ns,
                                uint32_t               req_id,
                                atclient_rpc_resp_type resp_type,
                                const char            *payload_json,
                                const char            *message,
                                uint32_t               ttl_ms);

#ifdef __cplusplus
}
#endif
#endif // ATCLIENT_RPC_H
