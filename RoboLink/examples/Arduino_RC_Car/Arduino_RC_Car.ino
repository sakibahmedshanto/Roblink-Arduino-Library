/*
 * RoboLink — Arduino RC Car (Bluetooth via HC-05 / HC-06)
 * =========================================================
 * FIX 1 - Timeout guard wraps mixing; motors stop on disconnect.
 * FIX 2 - Raw throttle (no abs) for proper reverse.
 * FIX 3 - abs(right) not abs(left) in right-motor branch.
 * FIX 4 - steerDir flips when throttle < 0 so backward-left = left.
 *
 * Hardware:
 *   Board  : Arduino Uno / Nano / Mega  (or any AVR Arduino)
 *   BT     : HC-05 or HC-06 module on SoftwareSerial
 *   Driver : L298N
 *
 *   Wiring:
 *     HC-05 TX  -> Arduino pin 10  (SoftwareSerial RX)
 *     HC-05 RX  -> Arduino pin 11  (SoftwareSerial TX)  *use voltage divider!
 *     HC-05 VCC -> 5V,  GND -> GND
 *
 *     L298N:
 *       ENA (left speed)   -> Arduino pin 5  (PWM)
 *       IN1 (left fwd)     -> Arduino pin 7
 *       IN2 (left rev)     -> Arduino pin 8
 *       ENB (right speed)  -> Arduino pin 6  (PWM)
 *       IN3 (right fwd)    -> Arduino pin 9
 *       IN4 (right rev)    -> Arduino pin 4
 *
 * App setup:
 *   1. Pair HC-05/HC-06 on your phone (default PIN: 1234 or 0000).
 *   2. Open the RoboLink app -> Bluetooth mode -> select the module.
 *   3. Add a Joystick widget -> keys "throttle" and "steer".
 *   4. Drive!
 */

#include <RoboLink.h>

#define _SS_MAX_RX_BUFF 128
#include <SoftwareSerial.h>

const int  BT_RX_PIN = 10;
const int  BT_TX_PIN = 11;
const long BT_BAUD   = 9600;

const int MOTOR_L_EN  = 5;
const int MOTOR_L_IN1 = 7;
const int MOTOR_L_IN2 = 8;
const int MOTOR_R_EN  = 6;
const int MOTOR_R_IN1 = 9;
const int MOTOR_R_IN2 = 4;

SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN);
RoboLinkSerial robolink;
unsigned long lastPrint = 0;

void setMotor(int enPin, int in1, int in2, int speed, int direction) {
    digitalWrite(in1, direction > 0 ? HIGH : LOW);
    digitalWrite(in2, direction < 0 ? HIGH : LOW);
    analogWrite(enPin, constrain(abs(speed), 0, 255));
}

void stopMotors(int enPin, int in1, int in2) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(enPin, 0);
}

void setup() {
    Serial.begin(115200);
    btSerial.begin(BT_BAUD);
    Serial.println(F("\n-- RoboLink Arduino RC Car (BT) --\n"));

    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    pinMode(MOTOR_L_EN,  OUTPUT);
    pinMode(MOTOR_R_EN,  OUTPUT);

    robolink.begin(btSerial, nullptr);
    robolink.setTimeoutMs(1000);
    Serial.println(F("Waiting for connection..."));
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
            Serial.print(F("T=")); Serial.print(throttle);
            Serial.print(F(" S=")); Serial.print(steer);
            Serial.print(F(" L=")); Serial.print(left);
            Serial.print(F(" R=")); Serial.println(right);
        }
    }
}
