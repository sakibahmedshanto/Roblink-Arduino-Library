/*
 * RoboLink — ESP32 RC Car (Bluetooth)
 * =====================================
 * Drive a 2-motor car from the RoboLink app via Bluetooth.
 * Uses a joystick widget with keys "joy_x" and "joy_y".
 *
 * 1. Set Bluetooth name and motor pins below.
 * 2. Upload to ESP32.
 * 3. Pair on phone, open app, add joystick, drive!
 */

#include <RoboLink.h>

/* ── Bluetooth name ───────────────────────────────────────────────────── */
const char* BT_NAME = "RoboLink";

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
RoboLinkBT robolink;
unsigned long lastPrint = 0;
int speedL = 0, speedR = 0;

/* ── PWM setup (ESP32 Core 2.x / 3.x compat) ────────────────────────── */
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  void setupPWM() { ledcAttach(MOTOR_L_EN, PWM_FREQ, PWM_RES); ledcAttach(MOTOR_R_EN, PWM_FREQ, PWM_RES); }
  void pwm(uint8_t pin, uint32_t d) { ledcWrite(pin, d); }
#else
  void setupPWM() { ledcSetup(CH_L, PWM_FREQ, PWM_RES); ledcSetup(CH_R, PWM_FREQ, PWM_RES); ledcAttachPin(MOTOR_L_EN, CH_L); ledcAttachPin(MOTOR_R_EN, CH_R); }
  void pwm(uint8_t pin, uint32_t d) { ledcWrite(pin == MOTOR_L_EN ? CH_L : CH_R, d); }
#endif

void setMotor(uint8_t en, uint8_t in1, uint8_t in2, int speed) {
    digitalWrite(in1, speed > 0 ? HIGH : LOW);
    digitalWrite(in2, speed < 0 ? HIGH : LOW);
    pwm(en, constrain(abs(speed), 0, 255));
}

/* ═════════════════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n── RoboLink RC Car (BT) ──\n"));

    pinMode(MOTOR_L_IN1, OUTPUT); pinMode(MOTOR_L_IN2, OUTPUT);
    pinMode(MOTOR_R_IN1, OUTPUT); pinMode(MOTOR_R_IN2, OUTPUT);
    setupPWM();

    if (!robolink.begin(BT_NAME)) {
        Serial.println(F("Bluetooth failed!")); while (true) delay(1000);
    }

    Serial.print(F("Bluetooth: ")); Serial.println(BT_NAME);
    Serial.println(F("Pair on phone, add joystick with \"joy_x\" and \"joy_y\".\n"));

    robolink.setTimeoutMs(500);
    robolink.setSendInterval(200);
}

/* ═════════════════════════════════════════════════════════════════════ */
void loop() {
    robolink.update();

    int jx = robolink.get("joy_x");
    int jy = robolink.get("joy_y");

    int L = constrain(map(jy + jx, -200, 200, -255, 255), -255, 255);
    int R = constrain(map(jy - jx, -200, 200, -255, 255), -255, 255);

    if (robolink.isTimedOut() || !robolink.hasReceivedData()) { L = R = 0; }

    speedL = L; speedR = R;
    setMotor(MOTOR_L_EN, MOTOR_L_IN1, MOTOR_L_IN2, L);
    setMotor(MOTOR_R_EN, MOTOR_R_IN1, MOTOR_R_IN2, R);

    robolink.setSensor("speed_l", speedL);
    robolink.setSensor("speed_r", speedR);
    robolink.setSensor("uptime",  (int)(millis() / 1000));

    if (millis() - lastPrint >= 3000) {
        lastPrint = millis();
        if (!robolink.hasClient()) {
            Serial.println(F("[BT] No device connected."));
        } else if (!robolink.hasReceivedData()) {
            Serial.println(F("[BT] Connected. Waiting for joystick..."));
        } else if (robolink.isTimedOut()) {
            Serial.println(F("[BT] Signal lost — motors stopped."));
        } else {
            Serial.print(F("[OK] L=")); Serial.print(speedL);
            Serial.print(F(" R="));     Serial.println(speedR);
        }
    }
}
