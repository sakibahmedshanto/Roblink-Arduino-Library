/*
 * RoboLink — ESP32 RC Car (WiFi)
 * =========================================================
 * FIX 1 - Timeout guard wraps mixing; motors stop on disconnect.
 * FIX 2 - Raw throttle (no abs) for proper reverse.
 * FIX 3 - abs(right) not abs(left) in right-motor branch.
 * FIX 4 - steerDir flips when throttle < 0 so backward-left = left.
 *
 * Hardware:
 *   Board  : ESP32 (any variant)
 *   Driver : L298N
 *
 *   Wiring:
 *     L298N:
 *       ENA (left speed)   -> ESP32 pin 12  (LEDC PWM)
 *       IN1 (left fwd)     -> ESP32 pin 27
 *       IN2 (left rev)     -> ESP32 pin 26
 *       ENB (right speed)  -> ESP32 pin 4   (LEDC PWM)
 *       IN3 (right fwd)    -> ESP32 pin 33
 *       IN4 (right rev)    -> ESP32 pin 32
 *
 * App setup:
 *   1. Connect phone to the WiFi network created by the ESP32.
 *   2. Open the RoboLink app -> WiFi mode -> connect.
 *   3. Add a Joystick widget -> keys "throttle" and "steer".
 *   4. Drive!
 */

#include <RoboLink.h>

/* ── WiFi credentials ─────────────────────────────────────────────────── */
const char* WIFI_SSID     = "RoboLink";
const char* WIFI_PASSWORD = "12345678";

/* ── Motor pins (L298N) ──────────────────────────────────────────────── */
const int MOTOR_L_EN  = 12;
const int MOTOR_L_IN1 = 27;
const int MOTOR_L_IN2 = 26;
const int MOTOR_R_EN  = 4;
const int MOTOR_R_IN1 = 33;
const int MOTOR_R_IN2 = 32;

/* ── PWM config ──────────────────────────────────────────────────────── */
const int PWM_FREQ = 5000;
const int PWM_RES  = 8;
const int CH_L = 0, CH_R = 1;

/* ── Objects ──────────────────────────────────────────────────────────── */
RoboLinkWiFi robolink;
unsigned long lastPrint = 0;

/* ── PWM setup (ESP32 Core 2.x / 3.x compat) ────────────────────────── */
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  void setupPWM() { ledcAttach(MOTOR_L_EN, PWM_FREQ, PWM_RES); ledcAttach(MOTOR_R_EN, PWM_FREQ, PWM_RES); }
  void pwmWrite(uint8_t pin, uint32_t d) { ledcWrite(pin, d); }
#else
  void setupPWM() { ledcSetup(CH_L, PWM_FREQ, PWM_RES); ledcSetup(CH_R, PWM_FREQ, PWM_RES); ledcAttachPin(MOTOR_L_EN, CH_L); ledcAttachPin(MOTOR_R_EN, CH_R); }
  void pwmWrite(uint8_t pin, uint32_t d) { ledcWrite(pin == MOTOR_L_EN ? CH_L : CH_R, d); }
#endif

void setMotor(uint8_t enPin, uint8_t in1, uint8_t in2, int speed, int direction) {
    digitalWrite(in1, direction > 0 ? HIGH : LOW);
    digitalWrite(in2, direction < 0 ? HIGH : LOW);
    pwmWrite(enPin, constrain(abs(speed), 0, 255));
}

void stopMotors(uint8_t enPin, uint8_t in1, uint8_t in2) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    pwmWrite(enPin, 0);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n-- RoboLink ESP32 RC Car (WiFi) --\n"));

    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    setupPWM();

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
        /*
         * steerDir = +1 when going forward, -1 when reversing.
         * This keeps steering intuitive in both directions.
         * Example: throttle=-50, steer=-30 (backward-left)
         *   Without fix: left=-50+(-30)=-80, right=-50-(-30)=-20
         *     -> left spins faster backward = car curves RIGHT (wrong!)
         *   With fix:    left=-50+(-30*-1)=-20, right=-50-(-30*-1)=-80
         *     -> right spins faster backward = car curves LEFT (correct!)
         */
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
