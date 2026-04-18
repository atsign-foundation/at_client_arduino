/**
 * BasicPKAMAuth.ino
 *
 * Minimal example of connecting to an atServer and authenticating with PKAM.
 *
 * This sketch:
 *   1. Connects to WiFi
 *   2. Initialises the atSDK
 *   3. Loads atKeys from the filesystem (LittleFS)
 *   4. PKAM-authenticates to the atServer
 *   5. Puts a self key and reads it back
 *
 * Prerequisites:
 *   - ESP32 board with WiFi
 *   - An already-activated atSign with an atKeys file
 *   - Upload your atkeys.json to the ESP32 data partition
 *     (PlatformIO: place in "data/" folder, run "pio run -t uploadfs")
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

// Your atSign (must match the atKeys file on the filesystem)
const char *ATSIGN = "@your_atsign";

// Path to the atKeys file on the ESP32 filesystem
const char *ATKEYS_PATH = "/atkeys.json";

// ============================================================================
// Globals
// ============================================================================

atclient client;
atclient_atkeys atkeys;

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=================================");
  Serial.println("  at_client – Basic PKAM Auth");
  Serial.println("=================================");

  // ── 1. Connect to WiFi ────────────────────────────────────────────────────
  Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());

  // ── 2. Initialise atSDK ───────────────────────────────────────────────────
  atsdk_arduino_setup();
  atlogger_set_logging_level(ATLOGGER_LOGGING_LEVEL_INFO);

  // ── 3. Load atKeys from the filesystem ────────────────────────────────────
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR: Failed to mount LittleFS");
    while (true) { delay(1000); }
  }

  atclient_atkeys_init(&atkeys);

  atclient_atkeys_file atkeys_file;
  atclient_atkeys_file_init(&atkeys_file);

  int ret = atclient_atkeys_file_from_path(&atkeys_file, ATKEYS_PATH);
  if (ret != 0) {
    Serial.printf("ERROR: Failed to read atkeys file (%d)\n", ret);
    Serial.println("Make sure you've uploaded atkeys.json to the data partition.");
    while (true) { delay(1000); }
  }

  ret = atclient_atkeys_populate_from_atkeys_file(&atkeys, &atkeys_file);
  atclient_atkeys_file_free(&atkeys_file);
  if (ret != 0) {
    Serial.printf("ERROR: Failed to populate atkeys (%d)\n", ret);
    while (true) { delay(1000); }
  }

  Serial.println("atKeys loaded from filesystem.");

  // ── 4. PKAM authenticate ──────────────────────────────────────────────────
  atclient_init(&client);

  atclient_authenticate_options auth_opts;
  atclient_authenticate_options_init(&auth_opts);

  ret = atclient_pkam_authenticate(&client, ATSIGN, &atkeys, &auth_opts, NULL);
  atclient_authenticate_options_free(&auth_opts);
  if (ret != 0) {
    Serial.printf("ERROR: PKAM authentication failed (%d)\n", ret);
    while (true) { delay(1000); }
  }

  Serial.printf("Authenticated as %s\n", ATSIGN);

  // ── 5. Put a self key ─────────────────────────────────────────────────────
  atclient_atkey key;
  atclient_atkey_init(&key);
  atclient_atkey_create_self_key(&key, "greeting", ATSIGN, NULL);

  int commit_id = 0;
  ret = atclient_put_self_key(&client, &key, "Hello from ESP32!", NULL, &commit_id);
  if (ret != 0) {
    Serial.printf("ERROR: put_self_key failed (%d)\n", ret);
  } else {
    Serial.printf("Put self key 'greeting' (commit %d)\n", commit_id);
  }

  // ── 6. Get the self key back ──────────────────────────────────────────────
  char *value = NULL;
  ret = atclient_get_self_key(&client, &key, &value, NULL);
  if (ret != 0) {
    Serial.printf("ERROR: get_self_key failed (%d)\n", ret);
  } else {
    Serial.printf("Got self key 'greeting' = \"%s\"\n", value);
    free(value);
  }

  atclient_atkey_free(&key);

  Serial.println("\nDone! Basic PKAM auth example complete.");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
  // Nothing to do – this is a one-shot example.
  // Send a heartbeat every 30 s to keep the connection alive.
  static unsigned long last = 0;
  if (millis() - last > 30000) {
    last = millis();
    atclient_send_heartbeat(&client);
    Serial.printf("[heartbeat] Free heap: %d\n", ESP.getFreeHeap());
  }
  delay(100);
}
