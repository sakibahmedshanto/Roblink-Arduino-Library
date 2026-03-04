/*
 * RoboLink — Print All Incoming Data (Arduino + HC-05/HC-06)
 * ============================================================
 * Prints every key-value pair arriving from the RoboLink app
 * via a Bluetooth module connected over SoftwareSerial.
 *
 * Hardware:
 *   Board : Arduino Uno / Nano / Mega
 *   BT    : HC-05 or HC-06 module
 *
 *   Wiring:
 *     HC-05 TX  → Arduino pin 10  (SoftwareSerial RX)
 *     HC-05 RX  → Arduino pin 11  (SoftwareSerial TX)  *voltage divider!
 *     HC-05 VCC → 5V,  GND → GND
 *
 * Baud rate:
 *   HC-06 = 9600 (default).  HC-05 = often 38400.
 *   Change BT_BAUD below if nothing arrives.
 *
 * Steps:
 *   1. Upload to Arduino.
 *   2. Open Serial Monitor at 115200 baud.
 *   3. Pair HC-05/HC-06 on phone, open RoboLink app → BT mode.
 *   4. Add any widgets — see data print here.
 */

#include <RoboLink.h>

/* Enlarge SoftwareSerial RX buffer (default 64 → 128) so incoming
 * BT bytes aren't lost while Serial.print() briefly blocks. */
#define _SS_MAX_RX_BUFF 128
#include <SoftwareSerial.h>

/* ── Bluetooth serial ─────────────────────────────────────────────────── */
const int  BT_RX_PIN = 10;
const int  BT_TX_PIN = 11;
const long BT_BAUD   = 9600;   /* HC-06 = 9600, HC-05 = often 38400 */

/* ── Objects ──────────────────────────────────────────────────────────── */
SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN);
RoboLinkSerial robolink;

unsigned long lastPrint = 0;

/* Called whenever a value changes — just print it */
void onChanged(const char* key, int value) {
    Serial.print(key);
    Serial.print(F(" = "));
    Serial.println(value);
}

/* ═════════════════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    btSerial.begin(BT_BAUD);

    Serial.println(F("\n── RoboLink Print All Data (Arduino BT) ──\n"));

    /*
     * begin(input, output):
     *   input  = btSerial (receive data from app)
     *   output = nullptr  (no sensor sends — avoids SoftwareSerial
     *                       TX blocking the RX interrupt)
     */
    robolink.begin(btSerial, nullptr);
    robolink.onReceive(onChanged);

    Serial.print(F("BT baud: ")); Serial.println(BT_BAUD);
    Serial.println(F("Pair HC-05/HC-06 on phone, then connect from the app.\n"));
}

/* ═════════════════════════════════════════════════════════════════════ */
void loop() {
    robolink.update();

    /* Lightweight status — minimal output to avoid blocking */
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();

        if (!robolink.hasReceivedData()) {
            Serial.print(F("[WAIT] b="));
            Serial.print(robolink.bytesReceived());
            Serial.print(F(" m="));
            Serial.println(robolink.messagesReceived());
        } else if (robolink.isTimedOut()) {
            Serial.println(F("[LOST]"));
        } else {
            Serial.print(F("[OK] "));
            Serial.print(robolink.dataCount());
            Serial.println(F(" keys"));
        }
    }

    /*
     * ── RAW BYTE DEBUG ──
     * Uncomment below to see every byte from BT as hex.
     * Readable ASCII = working.  Garbage = wrong baud rate.
     * Nothing at all = check wiring (HC-05 TX → pin 10).
     */
    // while (btSerial.available()) {
    //     uint8_t c = btSerial.read();
    //     if (c < 0x10) Serial.print('0');
    //     Serial.print(c, HEX);
    //     Serial.print(' ');
    // }
}
