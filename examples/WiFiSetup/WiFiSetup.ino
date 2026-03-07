/*
 * RoboLink — WiFi Setup (Serial Configurator)
 * ==============================================
 * Enter WiFi credentials via Serial Monitor. Saved to NVS.
 * Falls back to Access Point mode if no saved credentials.
 *
 * Serial commands:
 *   setup  — Enter SSID + password (saved to flash)
 *   clear  — Erase saved credentials
 *   status — Show connection info
 *   reboot — Restart ESP32
 */

#include <RoboLink.h>

/* ── Fallback AP (when no saved credentials) ──────────────────────────── */
const char* FALLBACK_SSID = "RoboLink_Setup";
const char* FALLBACK_PASS = "12345678";

/* ── Objects ──────────────────────────────────────────────────────────── */
RoboLinkWiFi robolink;
unsigned long lastPrint = 0;
int setupStep = 0;
String newSSID, newPass;

void onChanged(const char* key, int value) {
    Serial.print(key); Serial.print(F(" = ")); Serial.println(value);
}

/* ── Start WiFi: try saved, fallback to AP ────────────────────────────── */
void startWiFi() {
    Serial.println(F("Trying saved credentials..."));
    if (robolink.beginSaved()) {
        Serial.print(F("Connected! IP: ")); Serial.println(robolink.localIP());
        return;
    }
    Serial.println(F("No saved credentials. Starting fallback AP..."));
    if (robolink.beginAP(FALLBACK_SSID, FALLBACK_PASS)) {
        Serial.print(F("AP: ")); Serial.print(FALLBACK_SSID);
        Serial.print(F("  IP: ")); Serial.println(robolink.localIP());
    } else {
        Serial.println(F("AP failed!"));
    }
}

/* ── Handle serial input ──────────────────────────────────────────────── */
void handleSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    if (setupStep == 1) {
        newSSID = line;
        Serial.print(F("SSID: ")); Serial.println(newSSID);
        Serial.print(F("Password: "));
        setupStep = 2; return;
    }
    if (setupStep == 2) {
        newPass = line;
        Serial.println(F("Connecting..."));
        robolink.stop();
        if (robolink.beginSTA(newSSID.c_str(), newPass.c_str())) {
            robolink.saveCredentials(newSSID.c_str(), newPass.c_str());
            Serial.print(F("Connected & saved! IP: ")); Serial.println(robolink.localIP());
        } else {
            Serial.println(F("Failed. Restarting AP..."));
            robolink.beginAP(FALLBACK_SSID, FALLBACK_PASS);
        }
        setupStep = 0; return;
    }

    if (line.equalsIgnoreCase("setup")) {
        Serial.print(F("Enter SSID: ")); setupStep = 1;
    } else if (line.equalsIgnoreCase("clear")) {
        robolink.saveCredentials("", "");
        Serial.println(F("Credentials cleared. Type 'reboot' to restart."));
    } else if (line.equalsIgnoreCase("status")) {
        Serial.print(F("Mode: ")); Serial.println(robolink.isAPMode() ? F("AP") : F("STA"));
        Serial.print(F("IP: "));   Serial.println(robolink.localIP());
    } else if (line.equalsIgnoreCase("reboot")) {
        ESP.restart();
    } else {
        Serial.println(F("Commands: setup | clear | status | reboot"));
    }
}

/* ═════════════════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n── RoboLink WiFi Setup ──\n"));

    robolink.onReceive(onChanged);
    robolink.setSendInterval(200);
    startWiFi();

    Serial.println(F("\nType 'setup' to configure WiFi, 'status' for info.\n"));
}

/* ═════════════════════════════════════════════════════════════════════ */
void loop() {
    robolink.update();
    handleSerial();

    robolink.setSensor("uptime", (int)(millis() / 1000));
    robolink.setSensor("heap",   (int)(ESP.getFreeHeap() / 1024));
    robolink.setSensor("mode",   robolink.isAPMode() ? 1 : 0);

    if (millis() - lastPrint >= 5000) {
        lastPrint = millis();
        Serial.print(robolink.isAPMode() ? F("[AP] ") : F("[STA] "));
        Serial.println(robolink.localIP());
    }
}
