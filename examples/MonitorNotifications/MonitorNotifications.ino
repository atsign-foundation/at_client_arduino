/**
 * MonitorNotifications.ino
 *
 * Demonstrates how to listen for real-time notifications from the atServer
 * using the monitor connection, and how to send notifications to another atSign.
 *
 * The atProtocol monitor lets you subscribe to notifications matching a regex.
 * This is the mechanism used for real-time communication between atSigns.
 *
 * This sketch:
 *   1. Connects to WiFi and authenticates two connections:
 *      - A "worker" connection for sending commands (put, notify, etc.)
 *      - A "monitor" connection for receiving notifications
 *   2. Sends a test notification to another atSign
 *   3. Continuously reads notifications from the monitor connection
 *   4. Prints received notification details
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

// atSign to send a test notification to
const char *RECIPIENT_ATSIGN = "@their_atsign";

// Monitor regex – ".*" means all notifications, or use e.g. "test" to filter
const char *MONITOR_REGEX = ".*";

// ============================================================================
// Globals
// ============================================================================

atclient worker;       // for put/get/notify operations
atclient monitor_conn; // dedicated connection for receiving notifications
atclient_atkeys atkeys;

// ============================================================================
// Load keys helper
// ============================================================================

int load_atkeys() {
  atclient_atkeys_init(&atkeys);

  atclient_atkeys_file kf;
  atclient_atkeys_file_init(&kf);
  int ret = atclient_atkeys_file_from_path(&kf, ATKEYS_PATH);
  if (ret != 0) return ret;

  ret = atclient_atkeys_populate_from_atkeys_file(&atkeys, &kf);
  atclient_atkeys_file_free(&kf);
  return ret;
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("==========================================");
  Serial.println("  at_client – Monitor Notifications");
  Serial.println("==========================================");

  // ── 1. WiFi ───────────────────────────────────────────────────────────────
  Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());

  // ── 2. Init ───────────────────────────────────────────────────────────────
  atsdk_arduino_setup();
  atlogger_set_logging_level(ATLOGGER_LOGGING_LEVEL_INFO);
  if (!LittleFS.begin(true)) { Serial.println("LittleFS mount failed"); while(1) delay(1000); }

  if (load_atkeys() != 0) {
    Serial.println("Failed to load atKeys from filesystem!");
    while (true) delay(1000);
  }

  // ── 3. Authenticate worker connection ─────────────────────────────────────
  atclient_init(&worker);
  {
    atclient_authenticate_options opts;
    atclient_authenticate_options_init(&opts);
    int ret = atclient_pkam_authenticate(&worker, ATSIGN, &atkeys, &opts, NULL);
    atclient_authenticate_options_free(&opts);
    if (ret != 0) {
      Serial.printf("Worker PKAM auth failed: %d\n", ret);
      while (true) delay(1000);
    }
  }
  Serial.println("Worker connection authenticated.");

  // ── 4. Authenticate & start monitor connection ────────────────────────────
  //    The monitor MUST be a separate connection from the worker.
  atclient_monitor_init(&monitor_conn);
  {
    atclient_authenticate_options opts;
    atclient_authenticate_options_init(&opts);
    int ret = atclient_monitor_pkam_authenticate(&monitor_conn, ATSIGN, &atkeys, &opts);
    atclient_authenticate_options_free(&opts);
    if (ret != 0) {
      Serial.printf("Monitor PKAM auth failed: %d\n", ret);
      while (true) delay(1000);
    }
  }

  // Set a read timeout (e.g. 5 seconds) so monitor_read returns periodically
  atclient_monitor_set_read_timeout(&monitor_conn, 5000);

  // Start monitoring with a regex filter
  int ret = atclient_monitor_start(&monitor_conn, MONITOR_REGEX);
  if (ret != 0) {
    Serial.printf("Monitor start failed: %d\n", ret);
    while (true) delay(1000);
  }
  Serial.printf("Monitor started with regex: \"%s\"\n", MONITOR_REGEX);

  // ── 5. Send a test notification ───────────────────────────────────────────
  Serial.printf("\nSending test notification to %s ...\n", RECIPIENT_ATSIGN);
  {
    atclient_atkey key;
    atclient_atkey_init(&key);
    atclient_atkey_create_shared_key(&key, "test_notify", ATSIGN, RECIPIENT_ATSIGN, NULL);

    atclient_notify_params params;
    atclient_notify_params_init(&params);
    atclient_notify_params_set_atkey(&params, &key);
    atclient_notify_params_set_value(&params, "Hello from ESP32 monitor example!");
    atclient_notify_params_set_operation(&params, ATCLIENT_NOTIFY_OPERATION_UPDATE);

    char *notification_id = NULL;
    ret = atclient_notify(&worker, &params, &notification_id);
    if (ret == 0) {
      Serial.printf("Notification sent! ID: %s\n", notification_id ? notification_id : "(none)");
      if (notification_id) free(notification_id);
    } else {
      Serial.printf("Notification send failed: %d\n", ret);
    }

    atclient_notify_params_free(&params);
    atclient_atkey_free(&key);
  }

  Serial.println("\nListening for notifications...\n");
}

// ============================================================================
// Loop – poll for notifications
// ============================================================================

void loop() {
  // Check if monitor is still connected
  if (!atclient_monitor_is_connected(&monitor_conn)) {
    Serial.println("[MONITOR] Connection lost, reconnecting...");
    atclient_monitor_free(&monitor_conn);
    atclient_monitor_init(&monitor_conn);

    atclient_authenticate_options opts;
    atclient_authenticate_options_init(&opts);
    int ret = atclient_monitor_pkam_authenticate(&monitor_conn, ATSIGN, &atkeys, &opts);
    atclient_authenticate_options_free(&opts);
    if (ret != 0) {
      Serial.println("[MONITOR] Reconnect failed, retrying in 5s...");
      delay(5000);
      return;
    }
    atclient_monitor_set_read_timeout(&monitor_conn, 5000);
    atclient_monitor_start(&monitor_conn, MONITOR_REGEX);
    Serial.println("[MONITOR] Reconnected!");
  }

  // Read one notification (blocks up to read_timeout ms)
  atclient_monitor_message message;
  atclient_monitor_message_init(&message);

  int ret = atclient_monitor_read(&monitor_conn, &worker, &message, NULL);

  if (ret == 0) {
    switch (message.type) {

    case ATCLIENT_MONITOR_MESSAGE_TYPE_NOTIFICATION:
      if (message.notification != NULL) {
        Serial.println("[NOTIFICATION] ──────────────────────");
        Serial.printf("  ID:    %s\n", message.notification->id ? message.notification->id : "(null)");
        Serial.printf("  From:  %s\n", message.notification->from ? message.notification->from : "(null)");
        Serial.printf("  To:    %s\n", message.notification->to ? message.notification->to : "(null)");
        Serial.printf("  Key:   %s\n", message.notification->key ? message.notification->key : "(null)");
        Serial.printf("  Value: %s\n", message.notification->decrypted_value
                                           ? message.notification->decrypted_value : "(encrypted/null)");
        Serial.println("────────────────────────────────────");
      }
      break;

    case ATCLIENT_MONITOR_MESSAGE_TYPE_DATA_RESPONSE:
      Serial.printf("[DATA] %s\n", message.data_response ? message.data_response : "(null)");
      break;

    case ATCLIENT_MONITOR_MESSAGE_TYPE_ERROR_RESPONSE:
      Serial.printf("[ERROR] %s\n", message.error_response ? message.error_response : "(null)");
      break;

    case ATCLIENT_MONITOR_MESSAGE_TYPE_EMPTY:
      // No notification received within the timeout – this is normal
      break;

    default:
      break;
    }
  } else {
    // Non-zero return from monitor_read
    if (message.type == ATCLIENT_MONITOR_ERROR_READ) {
      if (message.error_read.error_code == MBEDTLS_ERR_SSL_TIMEOUT) {
        // Read timeout – normal, just loop again
      } else {
        Serial.printf("[MONITOR] Read error: %d\n", message.error_read.error_code);
      }
    }
  }

  atclient_monitor_message_free(&message);

  // Brief yield to RTOS
  delay(10);
}
