/**
 * @file atauth.h
 * @brief atauth - atsign activation & enrollment for Arduino/ESP32
 *
 * Provides two main workflows:
 *   1. Onboard  - activate a new atsign using a CRAM key (first-time setup)
 *   2. Enroll   - request a new set of APKAM keys for an already-activated atsign
 *
 * Usage (onboard):
 *   int ret = atauth_onboard_command("@myatsign", "root.atsign.org", "/keys.atKeys", "abc123cramkey");
 *
 * Usage (enroll):
 *   int ret = atauth_enroll_command("@myatsign", "root.atsign.org", "/keys.atKeys",
 *                                   "123456", "myApp", "myDevice", "ns1:rw,ns2:r", NULL);
 */
#ifndef ATAUTH_H
#define ATAUTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "atauth/run_onboard_command.h" // IWYU pragma: export
#include "atauth/run_enroll_command.h"  // IWYU pragma: export
#include "atauth/apkam_keys.h"         // IWYU pragma: export
#include "atauth/constants.h"           // IWYU pragma: export
#include "atauth/enroll_namespace.h"    // IWYU pragma: export
#include "atauth/enroll_operation.h"    // IWYU pragma: export
#include "atauth/enroll_params.h"       // IWYU pragma: export
#include "atauth/enroll_request.h"      // IWYU pragma: export
#include "atauth/enroll_response.h"     // IWYU pragma: export
#include "atauth/wait_for_enrollment.h" // IWYU pragma: export

#ifdef __cplusplus
}
#endif

#endif // ATAUTH_H
