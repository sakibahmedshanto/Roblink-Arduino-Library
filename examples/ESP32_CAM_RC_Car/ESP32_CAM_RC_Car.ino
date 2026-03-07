/*
 * RoboLink — ESP32-CAM RC Car (WiFi + Camera)
 * =============================================
 * Simple 4-direction RC car: Forward / Backward / Left / Right
 *
 * Hardware:
 *   Board  : AI-Thinker ESP32-CAM
 *   Driver : L298N
 *
 *   SPEED_PIN (GPIO 12) → ENA and ENB tied together (single speed pin)
 *   IN1       (GPIO 13) → Right motors terminal A
 *   IN2       (GPIO 15) → Right motors terminal B
 *   IN3       (GPIO 14) → Left motors terminal A
 *   IN4       (GPIO  2) → Left motors terminal B
 *
 * App setup:
 *   1. Connect phone to "RoboLink_CAM" WiFi.
 *   2. Add a Joystick widget  → keys "joy_x" and "joy_y"
 *   3. Add a Camera widget    → URL http://192.168.4.1:81/stream
 *   4. Drive!
 */

#include <RoboLink.h>

/* We need the camera driver — included with ESP32 Arduino core */
#include "esp_camera.h"
#include <WiFi.h>

/* ── WiFi credentials ─────────────────────────────────────────────────── */
const char* WIFI_SSID     = "RoboLink_CAM";
const char* WIFI_PASSWORD = "12345678";

/* ── Motor pins (L298N) ──────────────────────────────────────────────── */
const int SPEED_PIN = 12;   // ENA + ENB tied together → single speed pin
const int IN1       = 13;   // Right motors: forward
const int IN2       = 15;   // Right motors: backward
const int IN3       = 14;   // Left motors:  forward
const int IN4       =  2;   // Left motors:  backward

/* ── Camera pin definition — AI-Thinker ESP32-CAM ────────────────────── */
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

/* ── MJPEG stream server on port 81 ──────────────────────────────────── */
#define STREAM_PORT 81

/* ── RoboLink object ─────────────────────────────────────────────────── */
RoboLinkWiFi robolink;
unsigned long lastPrint = 0;

/* ===================================================================== */
/*  Motor commands                                                        */
/* ===================================================================== */

void motorForward(int speed) {
    analogWrite(SPEED_PIN, speed);
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void motorBackward(int speed) {
    analogWrite(SPEED_PIN, speed);
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}

void motorLeft(int speed) {
    analogWrite(SPEED_PIN, speed);
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);   // right forward
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);  // left backward
}

void motorRight(int speed) {
    analogWrite(SPEED_PIN, speed);
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);  // right backward
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);   // left forward
}

void motorStop() {
    analogWrite(SPEED_PIN, 0);
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

/* ===================================================================== */
/*  Camera initialisation                                                 */
/* ===================================================================== */
bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    /* Use lower resolution for smooth streaming while driving */
    if (psramFound()) {
        config.frame_size   = FRAMESIZE_VGA;    // 640×480
        config.jpeg_quality = 12;               // 0-63, lower = better
        config.fb_count     = 2;
    } else {
        config.frame_size   = FRAMESIZE_QVGA;   // 320×240
        config.jpeg_quality = 15;
        config.fb_count     = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }

    /* Optional: tweak sensor settings for better image */
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 1);     // -2 to 2
        s->set_saturation(s, 0);     // -2 to 2
    }

    Serial.println(F("[CAM] Camera OK"));
    return true;
}

/* ===================================================================== */
/*  MJPEG stream — runs on a SEPARATE CORE so it never blocks loop()      */
/* ===================================================================== */
/*
 * The stream task runs on Core 0 with its own WiFiServer.  The main
 * loop() (motor control + UDP) runs on Core 1 as usual.  This way a
 * blocking while(client.connected()) send-loop is perfectly fine —
 * it cannot starve the motor/UDP code.
 */
static const char* STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char* STREAM_CONTENT  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

void streamTask(void* pvParameters) {
    WiFiServer server(STREAM_PORT);
    server.begin();
    Serial.println(F("[CAM] MJPEG server started on port 81 (Core 0)"));

    for (;;) {
        WiFiClient client = server.available();
        if (!client) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        Serial.println(F("[CAM] Client connected"));

        /* Wait for the HTTP request line (we don't parse it, just drain it) */
        unsigned long t0 = millis();
        while (client.connected() && !client.available()) {
            if (millis() - t0 > 3000) break;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        while (client.available()) client.read(); // drain request

        /* Send HTTP response header */
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
        client.println("Access-Control-Allow-Origin: *");
        client.println("Connection: keep-alive");
        client.println();

        /* Stream frames until the client disconnects */
        while (client.connected()) {
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb) {
                Serial.println(F("[CAM] Frame capture failed"));
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            char headerBuf[64];
            snprintf(headerBuf, sizeof(headerBuf), STREAM_CONTENT, fb->len);

            client.print(STREAM_BOUNDARY);
            client.print(headerBuf);
            client.write(fb->buf, fb->len);

            esp_camera_fb_return(fb);

            /* Yield to other tasks on this core */
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        Serial.println(F("[CAM] Client disconnected"));
    }
}



/* ===================================================================== */
/*  setup()                                                               */
/* ===================================================================== */
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n══════════════════════════════════════════"));
    Serial.println(F("  RoboLink — ESP32-CAM RC Car"));
    Serial.println(F("══════════════════════════════════════════\n"));

    /* ── Motor pins ─────────────────────────────────────────────────── */
    pinMode(SPEED_PIN, OUTPUT);
    pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
    motorStop();

    /* ── Camera ─────────────────────────────────────────────────────── */
    if (!initCamera()) {
        Serial.println(F("!! Camera init failed — continuing without video."));
    }

    /* ── WiFi Access Point ──────────────────────────────────────────── */
    if (!robolink.beginAP(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println(F("WiFi AP failed!"));
        while (true) delay(1000);
    }

    Serial.print(F("WiFi AP : ")); Serial.println(WIFI_SSID);
    Serial.print(F("IP      : ")); Serial.println(robolink.localIP());
    Serial.printf("Stream  : http://%s:%d/stream\n",
                  robolink.localIP().toString().c_str(), STREAM_PORT);

    /* Start MJPEG stream on Core 0 */
    xTaskCreatePinnedToCore(streamTask, "mjpeg", 4096, NULL, 1, NULL, 0);

    robolink.setTimeoutMs(500);
    robolink.setSendInterval(200);
}

/* ===================================================================== */
/*  loop()                                                                */
/* ===================================================================== */
void loop() {
    robolink.update();

    int throt = robolink.get("joy_y");  // -100 ... +100  (forward/backward)
    int steer = robolink.get("joy_x");  // -100 ... +100  (left/right)

    /* Stop on timeout or before first data */
    if (robolink.isTimedOut() || !robolink.hasReceivedData()) {
        motorStop();
        return;
    }

    /*
     * Simple 4-direction logic:
     * Whichever axis is pushed harder wins.
     * Speed is mapped from joystick (0-100) to PWM (0-255).
     */
    if (abs(throt) >= abs(steer)) {
        if (throt > 10) {
            motorForward(map(throt, 0, 100, 0, 255));
        } else if (throt < -10) {
            motorBackward(map(-throt, 0, 100, 0, 255));
        } else {
            motorStop();
        }
    } else {
        if (steer > 10) {
            motorRight(map(steer, 0, 100, 0, 255));
        } else if (steer < -10) {
            motorLeft(map(-steer, 0, 100, 0, 255));
        } else {
            motorStop();
        }
    }

    /* Send uptime back to app */
    robolink.setSensor("uptime", (int)(millis() / 1000));

    /* Periodic Serial status */
    if (millis() - lastPrint >= 500) {
        lastPrint = millis();
        if (robolink.clientCount() == 0) {
            Serial.println(F("[WAIT] No phone connected."));
        } else if (!robolink.hasReceivedData()) {
            Serial.println(F("[WAIT] Waiting for joystick..."));
        } else if (robolink.isTimedOut()) {
            Serial.println(F("[LOST] Signal lost — motors stopped."));
        } else {
            /* Determine current command direction */
            const char* dir = "STOP";
            if      (abs(throt) >= abs(steer) && throt >  10) dir = "FORWARD";
            else if (abs(throt) >= abs(steer) && throt < -10) dir = "BACKWARD";
            else if (abs(steer) >  abs(throt) && steer >  10) dir = "RIGHT";
            else if (abs(steer) >  abs(throt) && steer < -10) dir = "LEFT";

            Serial.printf("[IN] throttle=%d  steer=%d  => %s\n", throt, steer, dir);
        }
    }
}
