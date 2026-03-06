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
