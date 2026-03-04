/*
 * RoboLink — Print All Incoming Data (WiFi)
 * ==========================================
 * Prints every key-value pair arriving from the RoboLink app.
 * Also sends a few sensor values back to the app.
 *
 * 1. Upload to ESP32.
 * 2. Connect phone to the WiFi network below.
 * 3. Open the RoboLink app → start a layout → see data here.
 */

#include <RoboLink.h>

/* ── WiFi credentials ─────────────────────────────────────────────────── */
const char* WIFI_SSID     = "RoboLink";
const char* WIFI_PASSWORD = "12345678";

/* ── Objects ──────────────────────────────────────────────────────────── */
RoboLinkWiFi robolink;
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
    delay(500);

    Serial.println(F("\n── RoboLink Print All Data ──\n"));

    /* 1. Start WiFi Access Point */
    if (!robolink.beginAP(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println(F("WiFi AP failed!"));
        while (true) delay(1000);
    }

    Serial.print(F("WiFi AP started: ")); Serial.println(WIFI_SSID);
    Serial.print(F("IP: "));              Serial.println(robolink.localIP());
    Serial.println(F("Connect your phone to this network, then open the app.\n"));

    /* 2. Print every value change */
    robolink.onReceive(onChanged);

    /* 3. Sensor data to send back (auto-sent every 200 ms) */
    robolink.setSendInterval(200);
}

/* ═════════════════════════════════════════════════════════════════════ */
void loop() {
    robolink.update();

    /* Update outgoing sensor values */
    robolink.setSensor("uptime", (int)(millis() / 1000));
    robolink.setSensor("heap",   (int)(ESP.getFreeHeap() / 1024));

    /* Periodic status line every 3 s */
    if (millis() - lastPrint >= 3000) {
        lastPrint = millis();

        int clients = robolink.clientCount();

        if (clients == 0) {
            Serial.println(F("[WiFi] No phone connected yet."));
        } else if (!robolink.hasReceivedData()) {
            Serial.println(F("[WiFi] Phone connected. Waiting for app data..."));
        } else if (robolink.isTimedOut()) {
            Serial.println(F("[WiFi] App stopped sending."));
        } else {
            Serial.print(F("[OK] Receiving data — "));
            Serial.print(robolink.dataCount());
            Serial.println(F(" keys"));
        }
    }
}
