/*
 * RoboLink — Print All Incoming Data (Bluetooth)
 * ================================================
 * Prints every key-value pair arriving from the RoboLink app via Bluetooth.
 * Also sends a few sensor values back to the app.
 *
 * 1. Upload to ESP32.
 * 2. Pair the device on your phone (Bluetooth settings).
 * 3. Open the RoboLink app → Bluetooth mode → select device → see data here.
 */

#include <RoboLink.h>

/* ── Bluetooth name ───────────────────────────────────────────────────── */
const char* BT_NAME = "RoboLink";

/* ── Objects ──────────────────────────────────────────────────────────── */
RoboLinkBT robolink;
unsigned long lastPrint = 0;

/* Called whenever a value changes */
void onChanged(const char* key, int value) {
    Serial.print(key);
    Serial.print(F(" = "));
    Serial.println(value);
}

/* ═════════════════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n── RoboLink Print All Data (BT) ──\n"));

    if (!robolink.begin(BT_NAME)) {
        Serial.println(F("Bluetooth failed!"));
        while (true) delay(1000);
    }

    Serial.print(F("Bluetooth started: ")); Serial.println(BT_NAME);
    Serial.println(F("Pair on your phone, then connect from the app.\n"));

    robolink.onReceive(onChanged);
    robolink.setSendInterval(200);
}

/* ═════════════════════════════════════════════════════════════════════ */
void loop() {
    robolink.update();

    robolink.setSensor("uptime", (int)(millis() / 1000));
    robolink.setSensor("heap",   (int)(ESP.getFreeHeap() / 1024));

    if (millis() - lastPrint >= 3000) {
        lastPrint = millis();

        if (!robolink.hasClient()) {
            Serial.println(F("[BT] No device connected yet."));
        } else if (!robolink.hasReceivedData()) {
            Serial.println(F("[BT] Connected. Waiting for app data..."));
        } else if (robolink.isTimedOut()) {
            Serial.println(F("[BT] App stopped sending."));
        } else {
            Serial.print(F("[OK] Receiving data — "));
            Serial.print(robolink.dataCount());
            Serial.println(F(" keys"));
        }
    }
}
