/**
 * at_authenticate.ino
 *
 * Demonstrates the two atAuth workflows for creating new atKeys on an ESP32:
 *
 *   MODE 1 – ONBOARD (first-time activation)
 *     Uses a one-time CRAM key from the registrar to activate a brand-new
 *     atSign. Generates APKAM + encryption key pairs, enrolls them, and
 *     writes the atKeys file. The CRAM key is deleted from the server
 *     afterwards and cannot be reused.
 *
 *   MODE 2 – ENROLL (additional app/device enrollment)
 *     Uses an OTP (one-time passcode) obtained from an already-activated
 *     atSign to enroll a new app/device. This creates a new APKAM key pair
 *     with scoped namespace access and writes its own atKeys file.
 *     The enrollment must be approved by an existing enrolled app.
 *
 * This mirrors the at_authenticate flow from the Dart atSDK.
 *
 * Prerequisites:
 *   - ESP32 board with WiFi
 *   - For ONBOARD: a brand-new un-activated atSign + its CRAM key
 *   - For ENROLL:  an already-activated atSign + an OTP from an approved app
 *   - LittleFS filesystem (for writing the generated atKeys)
 *
 * IMPORTANT: Replace the placeholder values below with your own.
 *            Set AUTH_MODE to 1 (onboard) or 2 (enroll).
 */

#include <WiFi.h>
#include <LittleFS.h>
#include <atsdk.h>

// ============================================================================
// Configuration – REPLACE THESE VALUES
// ============================================================================

const char *WIFI_SSID     = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Set AUTH_MODE:
//   1 = Onboard  (first-time activation with CRAM key)
//   2 = Enroll   (additional device/app enrollment with OTP)
#define AUTH_MODE 1

// The atSign to authenticate
const char *ATSIGN = "@your_atsign";

// atDirectory root server
const char *ROOT_DOMAIN = "root.atsign.org";

// Where to write the generated atKeys on the filesystem
const char *ATKEYS_PATH = "/atkeys.json";

// ── MODE 1: Onboard settings ───────────────────────────────────────────────
// The one-time CRAM key from the atSign registrar
const char *CRAM_KEY = "PASTE_YOUR_CRAM_KEY_HERE";

// ── MODE 2: Enroll settings ────────────────────────────────────────────────
// OTP obtained from an already-enrolled app (e.g. atDashboard or at_activate)
const char *ENROLL_OTP = "PASTE_YOUR_OTP_HERE";

// App and device name for the enrollment (identifies this key pair)
const char *APP_NAME    = "my_esp32_app";
const char *DEVICE_NAME = "esp32";

// Namespace access requested (format: "namespace:access,namespace:access")
// Use "__global" for all namespaces, "rw" for read-write, "r" for read-only
const char *NAMESPACES = "__global:rw";

// Key expiry in milliseconds (0 = no expiry)
// Example: "86400000" = 24 hours
const char *EXPIRY = "0";

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("==========================================");
  Serial.println("  at_authenticate – Create New atKeys");
  Serial.println("==========================================");

  // ── 1. Connect to WiFi ────────────────────────────────────────────────────
  Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());

  // ── 2. Initialise atSDK & filesystem ──────────────────────────────────────
  atsdk_arduino_setup();
  atlogger_set_logging_level(ATLOGGER_LOGGING_LEVEL_INFO);

  if (!LittleFS.begin(true)) {
    Serial.println("ERROR: Failed to mount LittleFS");
    while (true) delay(1000);
  }

  Serial.printf("atSign:    %s\n", ATSIGN);
  Serial.printf("Keys path: %s\n", ATKEYS_PATH);
  Serial.printf("Free heap: %u bytes\n\n", ESP.getFreeHeap());

  int ret = -1;

#if AUTH_MODE == 1
  // ── 3a. ONBOARD – first-time activation ────────────────────────────────
  Serial.println("Mode: ONBOARD (first-time activation with CRAM key)");
  Serial.println("──────────────────────────────────────────────────────");
  Serial.println("This will:");
  Serial.println("  • CRAM-authenticate with the atServer");
  Serial.println("  • Generate fresh APKAM + encryption key pairs");
  Serial.println("  • Enroll the keys via enroll:request");
  Serial.println("  • PKAM-authenticate with the new keys");
  Serial.println("  • Upload the default encryption public key");
  Serial.println("  • Delete the one-time CRAM secret from the server");
  Serial.println("  • Write the new atKeys file to the filesystem");
  Serial.println();

  ret = atauth_onboard_command(ATSIGN, ROOT_DOMAIN, ATKEYS_PATH, CRAM_KEY);

  if (ret != 0) {
    Serial.printf("\nERROR: Onboard failed with code %d\n", ret);
    Serial.println("Check that:");
    Serial.println("  • The CRAM key is correct and hasn't been used before");
    Serial.println("  • The atSign is spelled correctly (include the @)");
    Serial.println("  • WiFi can reach root.atsign.org");
    while (true) delay(1000);
  }

  Serial.println("\n=== ONBOARD COMPLETE ===");
  Serial.println("The CRAM key has been deleted from the server.");
  Serial.println("The atKeys file is now the ONLY way to authenticate.");

#elif AUTH_MODE == 2
  // ── 3b. ENROLL – additional app/device ─────────────────────────────────
  Serial.println("Mode: ENROLL (additional app/device with OTP)");
  Serial.println("──────────────────────────────────────────────────────");
  Serial.printf("  App:        %s\n", APP_NAME);
  Serial.printf("  Device:     %s\n", DEVICE_NAME);
  Serial.printf("  Namespaces: %s\n", NAMESPACES);
  Serial.println();
  Serial.println("This will:");
  Serial.println("  • Generate a new APKAM key pair for this app/device");
  Serial.println("  • Submit an enrollment request with the OTP");
  Serial.println("  • Wait for approval from an existing enrolled app");
  Serial.println("  • Write the new atKeys file to the filesystem");
  Serial.println();

  ret = atauth_enroll_command(ATSIGN, ROOT_DOMAIN, ATKEYS_PATH,
                              ENROLL_OTP, APP_NAME, DEVICE_NAME,
                              NAMESPACES, EXPIRY);

  if (ret != 0) {
    Serial.printf("\nERROR: Enroll failed with code %d\n", ret);
    Serial.println("Check that:");
    Serial.println("  • The OTP is correct and hasn't expired");
    Serial.println("  • The atSign is already activated (onboarded)");
    Serial.println("  • An existing enrolled app can approve the request");
    while (true) delay(1000);
  }

  Serial.println("\n=== ENROLLMENT COMPLETE ===");

#else
  #error "AUTH_MODE must be 1 (onboard) or 2 (enroll)"
#endif

  // ── 4. Success ─────────────────────────────────────────────────────────
  Serial.println();
  Serial.printf("atKeys written to: %s\n", ATKEYS_PATH);
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println();
  Serial.println("You can now use these keys with BasicPKAMAuth to connect.");
  Serial.println("Keep the atKeys file safe – it is your credential.");

  // Verify the file was written
  File f = LittleFS.open(ATKEYS_PATH, "r");
  if (f) {
    Serial.printf("Verified: %s exists (%d bytes)\n", ATKEYS_PATH, f.size());
    f.close();
  } else {
    Serial.println("WARNING: Could not open atKeys file for verification!");
  }
}

// ============================================================================
// Loop – nothing to do after authentication
// ============================================================================

void loop() {
  delay(10000);
}
