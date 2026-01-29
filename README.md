# IMAP SMS Forwarder

A lightweight ESP32 project that monitors an IMAP email inbox and forwards incoming emails via SMS using a SIM800L GSM module.

---

## Features

- Connects ESP32 to Wi-Fi and an IMAP email server over SSL.
- Monitors inbox periodically (every 15 minutes by default).
- Sends email **Subject** and **From** headers via SMS.
- Sends email **plain text body** via SMS with safe truncation to avoid SIM800L buffer overflow.
- Uses `HardwareSerial` for reliable SIM800L communication.
- Asynchronous IMAP handling via [ReadyMail](https://github.com/Whatnick/ReadyMail) library.

---

## Hardware Requirements

- ESP32 development board
- SIM800L GSM module
- Proper 5V/2A power supply for SIM800L
- Jumper wires

**Wiring Example:**

| SIM800L Pin | ESP32 Pin |
|-------------|-----------|
| TX          | 10 (SIM800L_RX_PIN) |
| RX          | 11 (SIM800L_TX_PIN) |
| GND         | GND       |
| VCC         | 5V (or external 4.2–5V supply) |

> Ensure correct power supply; SIM800L can draw up to 2A during transmission bursts.

---

## Software Requirements

- Arduino IDE 1.8.19 or newer
- ESP32 Arduino Core
- ReadyMail library
- Wi-Fi credentials and email account details in `secrets.h`  

`secrets.h` should define:

```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define IMAP_HOST "imap.yourmail.com"
#define IMAP_PORT 993
#define EMAIL_ACCOUNT "you@example.com"
#define EMAIL_PASSWORD "your_email_password"
#define PHONE_NUMBER "+1234567890"
````

---

## Usage

1. Connect ESP32 to Wi-Fi.
2. Connect SIM800L to ESP32 via hardware UART.
3. Flash the sketch.
4. The ESP32 checks the inbox every 15 minutes.

   * If a new email is detected, headers and body are sent via SMS.

---

## SMS Handling

* Each SMS is capped at **140 characters** for safe transmission.
* Long messages are truncated with `"..."` to avoid SIM800L overflow.
* SMS state (`sms_open`) ensures commands are not sent to the SIM800L before finishing previous SMS.

---

## Known Limitations

* Only **plain text** emails are forwarded; HTML-only emails are ignored.
* Long emails are **truncated** rather than split across multiple SMS.
* IMAP sequence numbers are used; messages deleted or moved can affect tracking.
* No error recovery if the SIM800L fails to send an SMS.

---

## Future Improvements

* **SMS splitting**: Automatically split long emails into multiple SMS messages (e.g., `[1/3]`, `[2/3]`, `[3/3]`).
* **HTML-to-text conversion**: Allow forwarding of HTML emails as readable plain text.
* **UID-based IMAP tracking**: More reliable detection of new emails even if messages are deleted or reordered.
* **SMS queue management**: Send emails asynchronously outside IMAP callbacks for higher reliability.
* **UTF-8 / multi-language support**: Ensure SMS handles Unicode properly.
