#include <RoboLink.h>

/* ── WiFi credentials ─────────────────────────────────────────────────── */
const char* WIFI_SSID     = "RoboLink";
const char* WIFI_PASSWORD = "12345678";

/* ── Motor pins (L298N) ──────────────────────────────────────────────── */
const int MOTOR_L_EN  = 0;
const int MOTOR_L_IN1 = 1;
const int MOTOR_L_IN2 = 2;

const int MOTOR_R_EN  = 5;
const int MOTOR_R_IN1 = 6;
const int MOTOR_R_IN2 = 7;
/* ── Objects ──────────────────────────────────────────────────────────── */
RoboLinkWiFi robolink;
unsigned long lastPrint = 0;

void setMotor(uint8_t enPin, uint8_t in1, uint8_t in2, int speed, int direction) {
    digitalWrite(in1, direction > 0 ? HIGH : LOW);
    digitalWrite(in2, direction < 0 ? HIGH : LOW);
    analogWrite(enPin, constrain(abs(speed), 0, 255));
}

void stopMotors(uint8_t enPin, uint8_t in1, uint8_t in2) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(enPin, 0);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n-- RoboLink ESP32 RC Car (WiFi) --\n"));

    pinMode(MOTOR_L_EN,  OUTPUT);
    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);
    pinMode(MOTOR_R_EN,  OUTPUT);
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);

    if (!robolink.beginAP(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println(F("WiFi AP failed!")); while (true) delay(1000);
    }

    Serial.print(F("WiFi AP: ")); Serial.println(WIFI_SSID);
    Serial.print(F("IP: "));      Serial.println(robolink.localIP());
    Serial.println(F("Waiting for connection..."));

    robolink.setTimeoutMs(500);
    robolink.setSendInterval(200);
}

void loop() {
    robolink.update();

    int throttle = robolink.get("throttle");  /* -100 ... +100 */
    int steer    = robolink.get("steer");     /* -100 ... +100 */

    int left  = 0;
    int right = 0;

    if (!robolink.isTimedOut() && robolink.hasReceivedData()) {

        int steerDir = (throttle >= -20) ? 1 : -1;
        left  = throttle + steer * steerDir;
        right = throttle - steer * steerDir;
    }

    /* Left motor */
    if      (left > 10)  setMotor(MOTOR_L_EN, MOTOR_L_IN1, MOTOR_L_IN2, abs(left),  1);
    else if (left < -10) setMotor(MOTOR_L_EN, MOTOR_L_IN1, MOTOR_L_IN2, abs(left), -1);
    else                 stopMotors(MOTOR_L_EN, MOTOR_L_IN1, MOTOR_L_IN2);

    /* Right motor */
    if      (right > 10)  setMotor(MOTOR_R_EN, MOTOR_R_IN1, MOTOR_R_IN2, abs(right),  1);
    else if (right < -10) setMotor(MOTOR_R_EN, MOTOR_R_IN1, MOTOR_R_IN2, abs(right), -1);
    else                  stopMotors(MOTOR_R_EN, MOTOR_R_IN1, MOTOR_R_IN2);

    robolink.setSensor("left",   left);
    robolink.setSensor("right",  right);
    robolink.setSensor("uptime", (int)(millis() / 1000));

    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        if (robolink.clientCount() == 0) {
            Serial.println(F("[WAIT] No phone connected."));
        } else if (!robolink.hasReceivedData()) {
            Serial.println(F("[WAIT] Connected. Waiting for joystick..."));
        } else if (robolink.isTimedOut()) {
            Serial.println(F("[LOST]"));
        } else {
            Serial.print(F("T=")); Serial.print(throttle);
            Serial.print(F(" S=")); Serial.print(steer);
            Serial.print(F(" L=")); Serial.print(left);
            Serial.print(F(" R=")); Serial.println(right);
        }
    }
}
