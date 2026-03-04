/*
 * RoboLink — Arduino RC Car (Bluetooth via HC-05 / HC-06)
 * =========================================================
 * Drive a 2-motor differential-drive car from the RoboLink app.
 * Uses a joystick widget with keys "joy_x" and "joy_y".
 *
 * Hardware:
 *   Board  : Arduino Uno / Nano / Mega  (or any AVR Arduino)
 *   BT     : HC-05 or HC-06 module on SoftwareSerial
 *   Driver : L298N
 *
 *   Wiring:
 *     HC-05 TX  → Arduino pin 10  (SoftwareSerial RX)
 *     HC-05 RX  → Arduino pin 11  (SoftwareSerial TX)  *use voltage divider!
 *     HC-05 VCC → 5V,  GND → GND
 *
 *     L298N:
 *       ENA (left speed)   → Arduino pin 5  (PWM)
 *       IN1 (left fwd)     → Arduino pin 7
 *       IN2 (left rev)     → Arduino pin 8
 *       ENB (right speed)  → Arduino pin 6  (PWM)
 *       IN3 (right fwd)    → Arduino pin 9
 *       IN4 (right rev)    → Arduino pin 4
 *
 *   ⚠ Pins 5 and 6 support analogWrite (PWM) on Uno/Nano/Mega.
 *     Do NOT use non-PWM pins for ENx — speed control won't work.
 *
 * Baud rate:
 *   HC-06 defaults to 9600.  HC-05 often defaults to 38400.
 *   If data never arrives, try changing BT_BAUD below.
 *   You can verify with the AT-command trick:
 *     1. Power the HC-05 while holding its button (EN pin HIGH)
 *        → it enters AT mode (LED blinks slowly).
 *     2. Open Serial Monitor and send "AT+UART?" to read the rate.
 *
 * ⚠ SoftwareSerial on AVR is HALF-DUPLEX — transmitting blocks
 *   receive.  This sketch therefore does NOT send sensor data back
 *   to the app.  If you need bidirectional data, use an Arduino
 *   Mega with HardwareSerial (Serial1/Serial2) — see the comment
 *   in setup() below.
 *
 * App setup:
 *   1. Pair HC-05/HC-06 on your phone (default PIN: 1234 or 0000).
 *   2. Open the RoboLink app → Bluetooth mode → select the module.
 *   3. Add a Joystick widget → keys "joy_x" and "joy_y".
 *   4. Drive!
 *
 * Differential drive mixing:
 *   left  = throttle + steer
 *   right = throttle − steer
 *   Both are proportionally scaled so the L/R ratio is preserved.
 */

#include <RoboLink.h>

/*
 * Enlarge SoftwareSerial RX ring-buffer (default 64 is too small).
 * At 9600 baud the app sends ~22 bytes every 50 ms; 128 bytes
 * gives ~6 messages of headroom so nothing is lost while
 * Serial.print() briefly occupies the CPU.
 */
#define _SS_MAX_RX_BUFF 128
#include <SoftwareSerial.h>

/* ── Bluetooth serial ─────────────────────────────────────────────────── */
const int BT_RX_PIN = 10;   /* Arduino RX ← HC-05 TX */
const int BT_TX_PIN = 11;   /* Arduino TX → HC-05 RX (use voltage divider!) */
const long BT_BAUD  = 9600; /* HC-06 = 9600, HC-05 = often 38400 */

/* ── Motor pins (L298N) ──────────────────────────────────────────────── */
const int MOTOR_L_EN  = 5;   /* PWM speed  — left  */
const int MOTOR_L_IN1 = 7;   /* direction  — left  */
const int MOTOR_L_IN2 = 8;
const int MOTOR_R_EN  = 6;   /* PWM speed  — right */
const int MOTOR_R_IN1 = 9;   /* direction  — right */
const int MOTOR_R_IN2 = 4;

/* ── Objects ──────────────────────────────────────────────────────────── */
SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN);
RoboLinkSerial robolink;

unsigned long lastPrint = 0;
int speedL = 0, speedR = 0;

/* ── Motor helper ─────────────────────────────────────────────────────── */
void setMotor(int enPin, int in1, int in2, int speed) {
    digitalWrite(in1, speed > 0 ? HIGH : LOW);
    digitalWrite(in2, speed < 0 ? HIGH : LOW);
    analogWrite(enPin, constrain(abs(speed), 0, 255));
}

/* ═════════════════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    btSerial.begin(BT_BAUD);

    Serial.println(F("\n── RoboLink Arduino RC Car (BT) ──\n"));

    /* Motor pins */
    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    pinMode(MOTOR_L_EN,  OUTPUT);
    pinMode(MOTOR_R_EN,  OUTPUT);

    /*
     * begin(input, output):
     *   input  = btSerial  (receive app data)
     *   output = nullptr   (disable sensor sends — avoids
     *                        SoftwareSerial TX blocking RX)
     *
     * ── Want bidirectional data? ──
     * On Arduino Mega, use HardwareSerial instead:
     *
     *     Serial1.begin(BT_BAUD);           // Mega pins 18(TX1) / 19(RX1)
     *     robolink.begin(Serial1);          // single stream = full duplex
     *     robolink.setSendInterval(200);    // auto-send works on HW UART
     *
     * ── SoftwareSerial bidirectional (advanced, Uno) ──
     * If you MUST send sensor data back through SoftwareSerial,
     * use a long interval to minimise RX data loss:
     *
     *     robolink.begin(btSerial);         // same stream for both
     *     robolink.setSendInterval(500);    // send every 500 ms
     *     // Half-duplex guard is ON by default on AVR — the library
     *     // will wait for a quiet RX period before sending.
     */
    robolink.begin(btSerial, nullptr);
    robolink.setTimeoutMs(1000);  /* generous: app interval adapts to payload size */

    Serial.print(F("BT baud: ")); Serial.println(BT_BAUD);
    Serial.println(F("Pair HC-05/HC-06, add joystick \"joy_x\" + \"joy_y\"."));
    Serial.println(F("Waiting for connection...\n"));
}

/* ═════════════════════════════════════════════════════════════════════ */
void loop() {
    robolink.update();

    /* ──────────────────────────────────────────────────────────────
     *  Differential Drive (same algorithm as ESP32 examples)
     * ──────────────────────────────────────────────────────────────
     *  joy_y = throttle (base speed)   –100 … +100
     *  joy_x = steer    (difference)   –100 … +100
     */
    int rawThrottle = robolink.get("joy_y");   /* –100 … +100 */
    int rawSteer    = robolink.get("joy_x");   /* –100 … +100 */

    /* Map joystick range to full PWM range */
    int throttle = map(rawThrottle, -100, 100, -255, 255);
    int steer    = map(rawSteer,    -100, 100, -255, 255);

    /* Small dead-zone */
    if (abs(throttle) < 10) throttle = 0;
    if (abs(steer)    < 10) steer    = 0;

    int left  = throttle + steer;
    int right = throttle - steer;

    /* Proportional scaling — keeps the L/R ratio intact */
    int peak = max(abs(left), abs(right));
    if (peak > 255) {
        left  = left  * 255 / peak;
        right = right * 255 / peak;
    }

    /* Safety: stop motors on timeout or before first data */
    if (robolink.isTimedOut() || !robolink.hasReceivedData()) {
        left = right = 0;
    }

    speedL = left;
    speedR = right;
    setMotor(MOTOR_L_EN, MOTOR_L_IN1, MOTOR_L_IN2, left);
    setMotor(MOTOR_R_EN, MOTOR_R_IN1, MOTOR_R_IN2, right);

    /*
     * Lightweight status — kept minimal so Serial.print() never
     * blocks long enough for SoftwareSerial RX bytes to be lost.
     */
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
            Serial.print(F("L=")); Serial.print(speedL);
            Serial.print(F(" R=")); Serial.println(speedR);
        }
    }

    /*
     * ── RAW BYTE DEBUG ──
     * Uncomment the block below to see every byte arriving from
     * the BT module as hex in Serial Monitor.  Useful to verify:
     *   • The BT module is wired correctly
     *   • The baud rate matches
     *   • The app is actually sending data
     *
     * You should see readable ASCII like:
     *   6A 6F 79 5F 78 3A 30 ...   ("joy_x:0...")
     * If you see garbage (random hex), the baud rate is wrong.
     * If you see nothing, check BT TX → Arduino pin 10 wiring.
     */
    // while (btSerial.available()) {
    //     uint8_t c = btSerial.read();
    //     if (c < 0x10) Serial.print('0');
    //     Serial.print(c, HEX);
    //     Serial.print(' ');
    // }
}
