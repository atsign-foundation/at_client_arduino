/**
 * PutGetKeys.ino
 *
 * Demonstrates all three key types in the atProtocol:
 *   - Self keys    (private, encrypted for you only)
 *   - Public keys  (visible to anyone, not encrypted)
 *   - Shared keys  (encrypted for you AND a specific other atSign)
 *
 * Also shows how to delete a key and how to scan for existing keys.
 *
 * This sketch:
 *   1. Connects to WiFi and authenticates via PKAM
 *   2. Puts and gets a self key
 *   3. Puts and gets a public key
 *   4. Puts and gets a shared key (shared with another atSign)
 *   5. Scans for all keys matching a regex
 *   6. Deletes a key
 *
 * Prerequisites:
 *   - ESP32 board with WiFi
 *   - An activated atSign with atKeys on the filesystem
 *   - Upload atkeys.json via "pio run -t uploadfs"
 *
 * IMPORTANT: Replace the placeholder values below with your own.
 */

#include <WiFi.h>
#include <LittleFS.h>
#include <atsdk.h>

// ============================================================================
// Configuration – REPLACE THESE VALUES
// ============================================================================

const char *WIFI_SSID     = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char *ATSIGN        = "@your_atsign";
const char *ATKEYS_PATH   = "/atkeys.json";

// Another atSign to share a key with (for the shared-key demo)
const char *RECIPIENT_ATSIGN = "@their_atsign";

// ============================================================================
// Globals
// ============================================================================

atclient client;
atclient_atkeys atkeys;

// ============================================================================
// Helper – authenticate and return 0 on success
// ============================================================================

int authenticate() {
  atclient_atkeys_init(&atkeys);

  atclient_atkeys_file kf;
  atclient_atkeys_file_init(&kf);
  int ret = atclient_atkeys_file_from_path(&kf, ATKEYS_PATH);
  if (ret != 0) return ret;

  ret = atclient_atkeys_populate_from_atkeys_file(&atkeys, &kf);
  atclient_atkeys_file_free(&kf);
  if (ret != 0) return ret;

  atclient_init(&client);
  atclient_authenticate_options opts;
  atclient_authenticate_options_init(&opts);
  ret = atclient_pkam_authenticate(&client, ATSIGN, &atkeys, &opts, NULL);
  atclient_authenticate_options_free(&opts);
  return ret;
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=================================");
  Serial.println("  at_client – Put / Get Keys");
  Serial.println("=================================");

  // WiFi
  Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());

  // Init
  atsdk_arduino_setup();
  atlogger_set_logging_level(ATLOGGER_LOGGING_LEVEL_INFO);
  if (!LittleFS.begin(true)) { Serial.println("LittleFS mount failed"); while(1) delay(1000); }

  // Auth
  if (authenticate() != 0) {
    Serial.println("PKAM auth failed!");
    while (true) delay(1000);
  }
  Serial.printf("Authenticated as %s\n\n", ATSIGN);

  int ret;
  int commit_id;
  char *value = NULL;

  // ── 1. Self key (private, encrypted for you only) ─────────────────────────
  Serial.println("=== SELF KEY ===");
  {
    atclient_atkey key;
    atclient_atkey_init(&key);
    atclient_atkey_create_self_key(&key, "temperature", ATSIGN, NULL);

    ret = atclient_put_self_key(&client, &key, "23.5", NULL, &commit_id);
    Serial.printf("  put self:temperature = \"23.5\" → %s (commit %d)\n",
                  ret == 0 ? "OK" : "FAIL", commit_id);

    value = NULL;
    ret = atclient_get_self_key(&client, &key, &value, NULL);
    Serial.printf("  get self:temperature → %s = \"%s\"\n",
                  ret == 0 ? "OK" : "FAIL", value ? value : "(null)");
    if (value) free(value);

    atclient_atkey_free(&key);
  }
  Serial.println();

  // ── 2. Public key (visible to everyone, not encrypted) ────────────────────
  Serial.println("=== PUBLIC KEY ===");
  {
    atclient_atkey key;
    atclient_atkey_init(&key);
    atclient_atkey_create_public_key(&key, "location", ATSIGN, NULL);

    // Mark metadata as public
    atclient_atkey_metadata_set_is_public(&key.metadata, true);

    ret = atclient_put_public_key(&client, &key, "ESP32 Lab", NULL, &commit_id);
    Serial.printf("  put public:location = \"ESP32 Lab\" → %s (commit %d)\n",
                  ret == 0 ? "OK" : "FAIL", commit_id);

    value = NULL;
    ret = atclient_get_public_key(&client, &key, &value, NULL);
    Serial.printf("  get public:location → %s = \"%s\"\n",
                  ret == 0 ? "OK" : "FAIL", value ? value : "(null)");
    if (value) free(value);

    atclient_atkey_free(&key);
  }
  Serial.println();

  // ── 3. Shared key (encrypted for you AND the recipient) ───────────────────
  Serial.println("=== SHARED KEY ===");
  {
    atclient_atkey key;
    atclient_atkey_init(&key);
    atclient_atkey_create_shared_key(&key, "secret_msg", ATSIGN, RECIPIENT_ATSIGN, NULL);

    ret = atclient_put_shared_key(&client, &key, "Hello from ESP32!", NULL, &commit_id);
    Serial.printf("  put %s:secret_msg%s = \"Hello from ESP32!\" → %s (commit %d)\n",
                  RECIPIENT_ATSIGN, ATSIGN, ret == 0 ? "OK" : "FAIL", commit_id);

    value = NULL;
    ret = atclient_get_shared_key(&client, &key, &value, NULL);
    Serial.printf("  get shared:secret_msg → %s = \"%s\"\n",
                  ret == 0 ? "OK" : "FAIL", value ? value : "(null)");
    if (value) free(value);

    atclient_atkey_free(&key);
  }
  Serial.println();

  // ── 4. Scan for keys ──────────────────────────────────────────────────────
  Serial.println("=== SCAN KEYS ===");
  {
    atclient_atkey *key_array = NULL;
    size_t array_len = 0;

    atclient_get_atkeys_request_options scan_opts;
    atclient_get_atkeys_request_options_init(&scan_opts);
    // You can set a regex, e.g. "temperature" to filter

    ret = atclient_get_atkeys(&client, &key_array, &array_len, &scan_opts);
    if (ret == 0) {
      Serial.printf("  Found %zu keys:\n", array_len);
      for (size_t i = 0; i < array_len && i < 10; i++) {
        Serial.printf("    [%zu] %s\n", i, key_array[i].name);
        atclient_atkey_free(&key_array[i]);
      }
      if (array_len > 10) {
        Serial.printf("    ... and %zu more\n", array_len - 10);
        for (size_t i = 10; i < array_len; i++) {
          atclient_atkey_free(&key_array[i]);
        }
      }
      free(key_array);
    } else {
      Serial.printf("  Scan failed: %d\n", ret);
    }
  }
  Serial.println();

  // ── 5. Delete a key ───────────────────────────────────────────────────────
  Serial.println("=== DELETE KEY ===");
  {
    atclient_atkey key;
    atclient_atkey_init(&key);
    atclient_atkey_create_self_key(&key, "temperature", ATSIGN, NULL);

    ret = atclient_delete(&client, &key, NULL, &commit_id);
    Serial.printf("  delete self:temperature → %s (commit %d)\n",
                  ret == 0 ? "OK" : "FAIL", commit_id);

    atclient_atkey_free(&key);
  }

  Serial.println("\n=== All done! ===");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
  delay(10000);
}
