/*
 * RoboLink — Sensor Data Sender (WiFi)
 * ======================================
 * Reads sensors on the ESP32 and sends them to the RoboLink app.
 * Also prints any data coming from the app (bidirectional demo).
 *
 * 1. Set WiFi credentials and sensor pins below.
 * 2. Upload to ESP32.
 * 3. Connect phone → open app → sensor gauge shows the values.
 */

#include <RoboLink.h>

/* ── WiFi credentials ─────────────────────────────────────────────────── */
const char* WIFI_SSID     = "RoboLink";
const char* WIFI_PASSWORD = "12345678";

/* ── Sensor pins ─────────────────────────────────────────────────────── */
const int PIN_POT    = 34;   /* Potentiometer (analog) */
const int PIN_BUTTON = 18;   /* Push-button (active LOW) */

/* ── Objects ──────────────────────────────────────────────────────────── */
RoboLinkWiFi robolink;
unsigned long lastPrint = 0;
float simTemp = 22.0;

/* Print data arriving from the app */
void onChanged(const char* key, int value) {
    Serial.print(F("[app] ")); Serial.print(key);
    Serial.print(F(" = "));   Serial.println(value);
}

/* ═════════════════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n── RoboLink Sensor Data ──\n"));

    pinMode(PIN_POT, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    if (!robolink.beginAP(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println(F("WiFi AP failed!")); while (true) delay(1000);
    }

    Serial.print(F("WiFi AP: ")); Serial.println(WIFI_SSID);
    Serial.print(F("IP: "));      Serial.println(robolink.localIP());
    Serial.println(F("Open the sensor gauge in the app to view data.\n"));

    robolink.onReceive(onChanged);
    robolink.setSendInterval(200);   /* auto-send sensors every 200 ms */
}

/* ═════════════════════════════════════════════════════════════════════ */
void loop() {
    robolink.update();

    /* Read hardware */
    int potPct = map(analogRead(PIN_POT), 0, 4095, 0, 100);
    int button = digitalRead(PIN_BUTTON) == LOW ? 1 : 0;

    /* Simulated temperature drift */
    simTemp += random(-10, 11) * 0.01;

    /* Set sensor values — library sends them automatically */
    robolink.setSensor("pot",    potPct);
    robolink.setSensor("button", button);
    robolink.setSensor("temp",   (int)(simTemp * 10));
    robolink.setSensor("heap",   (int)(ESP.getFreeHeap() / 1024));
    robolink.setSensor("uptime", (int)(millis() / 1000));

    /* Status every 3 s */
    if (millis() - lastPrint >= 3000) {
        lastPrint = millis();

        if (robolink.clientCount() == 0) {
            Serial.println(F("[WiFi] No phone connected."));
        } else {
            Serial.print(F("[OK] pot=")); Serial.print(potPct);
            Serial.print(F("% btn="));   Serial.print(button);
            Serial.print(F(" temp="));   Serial.print(simTemp, 1);
            Serial.println(F("C"));
        }
    }
}
