#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_IMAP
#define ENABLE_DEBUG
#include <ReadyMail.h>

#include "secrets.h"

#define SIM800L_RX_PIN 10
#define SIM800L_TX_PIN 11

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);
HardwareSerial sim(2);  // UART2

uint32_t last_seen_msg = 0;
bool sms_open = false;
size_t sms_len = 0;
#define SMS_MAX_LEN 140  // safe single-SMS payload

/* SMS helper functions */
void smsBegin() {
  if (sms_open) {
    Serial.println("[SMS] SMS already open, cannot begin new SMS.");
    return;
  }
  sim.println("AT+CMGF=1");
  sim.println(String("AT+CMGS=\"") + PHONE_NUMBER + "\"");
  sms_open = true;
  sms_len = 0;
}

/* Send a line of SMS */
void smsSendLine(const String& line) {
  if (!sms_open) {
    Serial.println("[SMS] SMS not open, cannot send line.");
    return;
  }

  if (sms_len + line.length() + 1 >= SMS_MAX_LEN) {
    size_t available = SMS_MAX_LEN - sms_len - 4;  // 4 for "...\r\n"
    String truncated_line = line.substring(0, available) + "...";
    sim.println(truncated_line);
    sms_len += truncated_line.length() + 1;  // +1 for newline
    return;
  }

  sim.println(line);
  sms_len += line.length() + 1;  // +1 for newline
}

/* End SMS message */
void smsEnd() {
  if (!sms_open) {
    Serial.println("[SMS] SMS not open, cannot end SMS.");
    return;
  }
  sim.write(26);  // Ctrl+Z
  sms_open = false;
}

/* IMAP status callback */
void imapStatusCallback(IMAPStatus status) {
  Serial.print("[IMAP STATUS] ");
  Serial.println(status.text);
}

/* IMAP data callback */
void imapDataCallback(IMAPCallbackData& data) {
  // Serial.print("[IMAP DATA] Event received: ");
  // Serial.println(data.event());

  // Handle fetch envelope event
  if (data.event() == imap_data_event_fetch_envelope) {
    Serial.println("\n--- LAST EMAIL ---");
    Serial.print("[IMAP DATA] Header count: ");
    Serial.println(data.headerCount());

    smsBegin();

    for (size_t i = 0; i < data.headerCount(); i++) {
      auto header = data.getHeader(i);
      Serial.printf("[HEADER] %s: %s\n",
                    header.first.c_str(),
                    header.second.c_str());
      // Send relevant headers via SMS
      if (header.first == "Subject" || header.first == "From") {
        smsSendLine(header.first + ": " + header.second);
      }
    }
  }

  if (data.event() == imap_data_event_fetch_body) {
    // if (data.fileInfo().mime == "text/plain" || data.fileInfo().mime == "text/html") {
    if (data.fileInfo().mime == "text/plain") {
      if (data.fileChunk().index == 0) {
        Serial.println("\n[EMAIL BODY] ------------------");
      }
      String line = (char*)data.fileChunk().data;
      Serial.print(line);
      smsSendLine(line);
      if (data.fileChunk().isComplete) {
        Serial.println("\n[EMAIL BODY END] --------------");
        smsEnd();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[SETUP] Starting ESP32...");

  Serial.println("[WIFI] Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("[WIFI] Trying to connect to wifi...");
  }
  Serial.println("[WIFI] Connected!");
  Serial.print("[WIFI] Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("[WIFI] Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("[WIFI] DNS IP: ");
  Serial.println(WiFi.dnsIP());

  /* SSL: skip certificate validation (simplest) */
  Serial.println("[SSL] Setting insecure mode...");
  ssl_client.setInsecure();

  /* Connect to IMAP server */
  Serial.print("[IMAP] Connecting to ");
  Serial.print(IMAP_HOST);
  Serial.print(":");
  Serial.println(IMAP_PORT);
  imap.connect(IMAP_HOST, IMAP_PORT, imapStatusCallback);

  if (!imap.isConnected()) {
    Serial.println("[IMAP] Connection failed!");
    return;
  }
  Serial.println("[IMAP] Connected successfully!");

  /* Authenticate */
  Serial.println("[IMAP] Authenticating...");
  imap.authenticate(
    EMAIL_ACCOUNT,
    EMAIL_PASSWORD,
    readymail_auth_password);
  Serial.println("[IMAP] Authentication complete.");

  /* Select inbox */
  Serial.println("[IMAP] Selecting INBOX...");
  imap.select("INBOX");

  // /* Fetch the latest message */
  // uint32_t lastMsg = imap.getMailbox().msgCount;
  // Serial.print("[IMAP] Total messages in INBOX: ");
  // Serial.println(lastMsg);
  // Serial.print("[IMAP] Fetching message #");
  // Serial.println(lastMsg);
  // imap.fetch(lastMsg, imapDataCallback);

  /* Store the last seen message count */
  last_seen_msg = imap.getMailbox().msgCount;
  Serial.print("[INIT] Last seen message: ");
  Serial.println(last_seen_msg);

  /* Initialize UART2 for SIM */
  sim.begin(9600, SERIAL_8N1, SIM800L_RX_PIN, SIM800L_TX_PIN);

  Serial.println("[SETUP] Setup complete.");
}

void loop() {
  // Every N seconds, check mailbox state:
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll > 900000) {  // 15 minutes interval
    lastPoll = millis();

    // Refresh mailbox to update .msgCount
    imap.select("INBOX");
    uint32_t curr_msg_count = imap.getMailbox().msgCount;

    if (curr_msg_count > last_seen_msg) {
      Serial.print("[NEW] New email detected! Fetching message #");
      Serial.println(curr_msg_count);
      imap.fetch(curr_msg_count, imapDataCallback);
      last_seen_msg = curr_msg_count;
    } else {
      Serial.println("[MONITOR] No new messages.");
    }
  }

  // ReadyMail is asynchronous, so yield or delay a little
  delay(100);
}
