/**
 * SendNotification.ino
 *
 * Demonstrates how to send notifications to another atSign using
 * atclient_notify(). Notifications are the primary mechanism for
 * real-time communication between atSigns.
 *
 * This sketch shows:
 *   1. Sending a simple encrypted notification (default)
 *   2. Sending a text message notification
 *   3. Sending a notification with custom priority and strategy
 *   4. Sending a delete notification
 *   5. Periodic sensor-style notifications in loop()
 *
 * The recipient will see these notifications if they are running a
 * monitor (see the MonitorNotifications example).
 *
 * Prerequisites:
 *   - ESP32 board with WiFi
 *   - An activated atSign with atKeys on LittleFS
 *   - Upload atkeys.json via "pio run -t uploadfs"
 *   - A recipient atSign (can be the same as yours for testing)
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

// The atSign to send notifications to
const char *RECIPIENT_ATSIGN = "@their_atsign";

// How often to send periodic notifications in loop() (milliseconds)
const unsigned long SEND_INTERVAL_MS = 30000; // 30 seconds

// ============================================================================
// Globals
// ============================================================================

atclient client;
atclient_atkeys atkeys;
unsigned long last_send_ms = 0;
int send_count = 0;

// ============================================================================
// Helper: load atkeys from filesystem
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
// Helper: send a notification and print the result
// ============================================================================

void send_notification(const char *key_name, const char *value,
                       enum atclient_notify_operation operation,
                       enum atclient_notify_message_type msg_type,
                       enum atclient_notify_priority priority) {

  atclient_atkey key;
  atclient_atkey_init(&key);
  atclient_atkey_create_shared_key(&key, key_name, ATSIGN, RECIPIENT_ATSIGN, NULL);

  atclient_notify_params params;
  atclient_notify_params_init(&params);
  atclient_notify_params_set_atkey(&params, &key);
  atclient_notify_params_set_operation(&params, operation);

  if (value != NULL) {
    atclient_notify_params_set_value(&params, value);
  }
  if (msg_type != ATCLIENT_NOTIFY_MESSAGE_TYPE_NONE) {
    atclient_notify_params_set_message_type(&params, msg_type);
  }
  if (priority != ATCLIENT_NOTIFY_PRIORITY_NONE) {
    atclient_notify_params_set_priority(&params, priority);
  }

  char *notification_id = NULL;
  int ret = atclient_notify(&client, &params, &notification_id);

  if (ret == 0) {
    Serial.printf("  OK   [%s] id=%s\n", key_name,
                  notification_id ? notification_id : "(none)");
    if (notification_id) free(notification_id);
  } else {
    Serial.printf("  FAIL [%s] error=%d\n", key_name, ret);
  }

  atclient_notify_params_free(&params);
  atclient_atkey_free(&key);
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("==========================================");
  Serial.println("  at_client – Send Notifications");
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
    Serial.println("Failed to load atKeys!");
    while (true) delay(1000);
  }

  // ── 3. Authenticate ──────────────────────────────────────────────────────
  atclient_init(&client);
  {
    atclient_authenticate_options opts;
    atclient_authenticate_options_init(&opts);
    int ret = atclient_pkam_authenticate(&client, ATSIGN, &atkeys, &opts, NULL);
    atclient_authenticate_options_free(&opts);
    if (ret != 0) {
      Serial.printf("PKAM auth failed: %d\n", ret);
      while (true) delay(1000);
    }
  }
  Serial.println("Authenticated.\n");

  // ── 4. Send various notification types ────────────────────────────────────

  Serial.printf("Sending notifications to %s ...\n\n", RECIPIENT_ATSIGN);

  // 4a. Simple encrypted notification (update)
  //     This is the most common use case — sending data to another atSign.
  //     The value is automatically encrypted with the shared key.
  Serial.println("─── Simple update notification ───");
  send_notification("greeting", "Hello from ESP32!",
                    ATCLIENT_NOTIFY_OPERATION_UPDATE,
                    ATCLIENT_NOTIFY_MESSAGE_TYPE_NONE,  // default: key
                    ATCLIENT_NOTIFY_PRIORITY_NONE);     // default: low

  // 4b. Text message notification
  //     Use MESSAGE_TYPE_TEXT for chat-style messages.
  Serial.println("─── Text message notification ───");
  send_notification("message", "This is a text message from ESP32",
                    ATCLIENT_NOTIFY_OPERATION_UPDATE,
                    ATCLIENT_NOTIFY_MESSAGE_TYPE_TEXT,
                    ATCLIENT_NOTIFY_PRIORITY_NONE);

  // 4c. High-priority notification
  //     Set priority and strategy for important messages.
  Serial.println("─── High-priority notification ───");
  {
    atclient_atkey key;
    atclient_atkey_init(&key);
    atclient_atkey_create_shared_key(&key, "alert", ATSIGN, RECIPIENT_ATSIGN, NULL);

    atclient_notify_params params;
    atclient_notify_params_init(&params);
    atclient_notify_params_set_atkey(&params, &key);
    atclient_notify_params_set_value(&params, "ALERT: High-priority event!");
    atclient_notify_params_set_operation(&params, ATCLIENT_NOTIFY_OPERATION_UPDATE);
    atclient_notify_params_set_priority(&params, ATCLIENT_NOTIFY_PRIORITY_HIGH);
    atclient_notify_params_set_strategy(&params, ATCLIENT_NOTIFY_STRATEGY_ALL);

    char *notification_id = NULL;
    int ret = atclient_notify(&client, &params, &notification_id);
    if (ret == 0) {
      Serial.printf("  OK   [alert] id=%s\n",
                    notification_id ? notification_id : "(none)");
      if (notification_id) free(notification_id);
    } else {
      Serial.printf("  FAIL [alert] error=%d\n", ret);
    }

    atclient_notify_params_free(&params);
    atclient_atkey_free(&key);
  }

  // 4d. Delete notification
  //     Tells the recipient to delete a previously shared key.
  Serial.println("─── Delete notification ───");
  send_notification("old_data", NULL,
                    ATCLIENT_NOTIFY_OPERATION_DELETE,
                    ATCLIENT_NOTIFY_MESSAGE_TYPE_NONE,
                    ATCLIENT_NOTIFY_PRIORITY_NONE);

  Serial.println("\n─── Setup notifications complete ───");
  Serial.printf("Now sending periodic sensor data every %lu seconds...\n\n",
                SEND_INTERVAL_MS / 1000);
}

// ============================================================================
// Loop – periodic sensor-style notifications
// ============================================================================

void loop() {
  unsigned long now = millis();
  if (now - last_send_ms >= SEND_INTERVAL_MS) {
    last_send_ms = now;
    send_count++;

    // Simulate sensor data
    float temperature = 20.0 + (float)(esp_random() % 150) / 10.0; // 20.0–35.0
    float humidity    = 40.0 + (float)(esp_random() % 400) / 10.0;  // 40.0–80.0
    uint32_t heap     = ESP.getFreeHeap();

    // Format as JSON
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"temp\":%.1f,\"humidity\":%.1f,\"heap\":%u,\"seq\":%d}",
             temperature, humidity, heap, send_count);

    Serial.printf("[%d] Sending sensor data: %s\n", send_count, payload);

    send_notification("sensor_data", payload,
                      ATCLIENT_NOTIFY_OPERATION_UPDATE,
                      ATCLIENT_NOTIFY_MESSAGE_TYPE_NONE,
                      ATCLIENT_NOTIFY_PRIORITY_NONE);
  }

  delay(100);
}
