#ifndef ATCOMMONS_ENROLL_PARAMS_H
#define ATCOMMONS_ENROLL_PARAMS_H

#include <stdlib.h>
typedef struct {
  char *enrollment_id;
  char *app_name;
  char *device_name;
  char *otp;
  struct enroll_namespace *namespaces; // list of enroll namespaces and their required access for current enrollment
  char *apkam_public_key;
  char *encrypted_default_encryption_private_key;    // apkam symmetric key encrypted default enc private key
  char *encrypted_default_encryption_private_key_iv; // IV that has been used to encrypt the default encryption
                                                     // private key
  char *encrypted_self_encryption_key;               // apkam symmetric key encrypted self enc key
  char *encrypted_self_encryption_key_iv;            // IV that has been used to encrypt the self encryption key
  char *encrypted_apkam_symmetric_key;
  char *encrypted_apkam_symmetric_key_iv;
  int apkam_keys_expiry_in_millis;
} atauth_enroll_params_t;

/**
 * @brief Initializes the enroll_params_t struct
 *
 * @param ep pointer to the enroll params struct that is to be initialized
 */
void atauth_enroll_params_init(atauth_enroll_params_t *ep);

/**
 * @brief Converts the parameters in an enroll_params_t struct to a json encoded string
 *
 * @param ep Pointer to the enroll_params_t struct whose values need to be converted to a json string
 * @param json_string Double pointer to store the json encoded string of provided enroll params
 * @return int 0 for success, non-zero int for failure
 */
int atauth_enroll_params_to_json(const atauth_enroll_params_t *ep, char **json_string);

#endif
