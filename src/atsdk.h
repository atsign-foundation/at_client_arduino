
#ifndef ATSDK_ARDUINO_H
#define ATSDK_ARDUINO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h> // IWYU pragma: export

#ifdef __cplusplus
}
#endif

void atsdk_arduino_setup();
#endif
#include "atlogger/atlogger.h" // IWYU pragma: export
#include "atchops/aes.h" // IWYU pragma: export
#include "atchops/aes_ctr.h" // IWYU pragma: export
#include "atchops/base64.h" // IWYU pragma: export
#include "atchops/constants.h" // IWYU pragma: export
#include "atchops/hex.h" // IWYU pragma: export
#include "atchops/iv.h" // IWYU pragma: export
#include "atchops/mbedtls.h" // IWYU pragma: export
#include "atchops/platform.h" // IWYU pragma: export
#include "atchops/rsa.h" // IWYU pragma: export
#include "atchops/rsa_key.h" // IWYU pragma: export
#include "atchops/sha.h" // IWYU pragma: export
#include "atchops/utf8.h" // IWYU pragma: export
#include "atchops/uuid.h" // IWYU pragma: export
#include "atcommons/memory_util.h" // IWYU pragma: export
#include "atclient/atclient.h" // IWYU pragma: export
#include "atclient/atclient_utils.h" // IWYU pragma: export
#include "atclient/atkey.h" // IWYU pragma: export
#include "atclient/atkeys.h" // IWYU pragma: export
#include "atclient/atkeys_file.h" // IWYU pragma: export
#include "atclient/atnotification.h" // IWYU pragma: export
#include "atclient/cacerts.h" // IWYU pragma: export
#include "atclient/connection.h" // IWYU pragma: export
#include "atclient/connection_hooks.h" // IWYU pragma: export
#include "atclient/constants.h" // IWYU pragma: export
#include "atclient/encryption_key_helpers.h" // IWYU pragma: export
#include "atclient/json.h" // IWYU pragma: export
#include "atclient/mbedtls.h" // IWYU pragma: export
#include "atclient/metadata.h" // IWYU pragma: export
#include "atclient/monitor.h" // IWYU pragma: export
#include "atclient/notify.h" // IWYU pragma: export
#include "atclient/notify_params.h" // IWYU pragma: export
#include "atclient/request_options.h" // IWYU pragma: export
#include "atclient/socket.h" // IWYU pragma: export
#include "atclient/socket_mbedtls.h" // IWYU pragma: export
#include "atclient/socket_shared.h" // IWYU pragma: export
#include "atclient/string_utils.h" // IWYU pragma: export
#include "atclient/version.h" // IWYU pragma: export
#include "atauth.h" // IWYU pragma: export
