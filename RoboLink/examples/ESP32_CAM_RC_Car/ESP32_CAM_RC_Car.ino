/*
 * RoboLink — ESP32-CAM RC Car (WiFi + Camera)
 * =============================================
 * Drive a 2-motor car from the RoboLink app with live camera feed.
 *
 * Features:
 *   • Differential drive via WiFi UDP joystick
 *       joy_y = throttle (base speed for both motors)
 *       joy_x = steer    (speed difference between L and R)
 *   • MJPEG camera stream on http://<IP>:81/stream
 *   • Flash LED toggle via key "light" (debounced)
 *
 * Hardware:
 *   Board  : AI-Thinker ESP32-CAM (or compatible)
 *   Driver : L298N
 *   RIGHT_MOTOR → EnA=12, IN1=13, IN2=15
 *   LEFT_MOTOR  → EnB=4,  IN3=14, IN4=2
 *
 *   GPIO 12 = right motor enable (PWM speed)
 *   GPIO 4  = left motor enable  (PWM speed)
 *   This gives independent speed per side (true differential drive).
 *
 *   ⚠  GPIO 4 is the flash LED pin on AI-Thinker boards.
 *      Using it for motor control means the flash LED is NOT available.
 *
 * App setup:
 *   1. Connect phone to the "RoboLink_CAM" WiFi network.
 *   2. Add a Joystick widget   → keys "joy_x" and "joy_y"
 *   3. Add a Camera widget     → URL  http://192.168.4.1:81/stream
 *   4. Drive!
 *
 * NOTE: The ESP32-CAM has very few free GPIOs. Pins 12, 13, 14, 15
 *       are usable when the SD-card slot is NOT in use.  GPIO 2 is
 *       shared with the on-board LED but works fine for motor output.
 *       GPIO 4 is used for left motor enable (differential speed).
 */

#include <RoboLink.h>

/* We need the camera driver — included with ESP32 Arduino core */
#include "esp_camera.h"
#include <WiFi.h>

/* ── WiFi credentials ─────────────────────────────────────────────────── */
const char* WIFI_SSID     = "RoboLink_CAM";
const char* WIFI_PASSWORD = "12345678";

/* ── Motor pins (L298N) ──────────────────────────────────────────────── */
//  {EnA, IN1, IN2}  — RIGHT motor
//  {EnB, IN3, IN4}  — LEFT motor
const int MOTOR_R_EN  = 12;
const int MOTOR_R_IN1 = 13;
const int MOTOR_R_IN2 = 15;

const int MOTOR_L_EN  = 4;    // separate enable pin (GPIO 4) for differential speed
const int MOTOR_L_IN1 = 14;
const int MOTOR_L_IN2 = 2;

/* ── Flash LED — NOT available (GPIO 4 used for motor enable) ────────── */
// const int FLASH_LED_PIN = 4;  // pin reassigned to MOTOR_L_EN

/* ── PWM config ──────────────────────────────────────────────────────── */
const int PWM_FREQ   = 5000;
const int PWM_RES    = 8;     // 0-255

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

/* ── MJPEG stream server on port 81 (runs on Core 0 as a FreeRTOS task) */
#define STREAM_PORT 81

/* ── RoboLink WiFi (UDP control on default port 4210) ────────────────── */
RoboLinkWiFi robolink;

/* ── State ───────────────────────────────────────────────────────────── */
unsigned long lastPrint = 0;
int speedL = 0, speedR = 0;

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

    /*
     * Disable the camera driver's internal flash LED control.
     * On AI-Thinker boards the camera driver may pulse GPIO 4
     * briefly during frame captures.  Since GPIO 4 is now used
     * for motor enable, we must prevent the camera driver from
     * interfering with it.
     */
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_aec2(sensor, 0);       // disable auto-exposure flash
    }

    Serial.println(F("[CAM] Camera initialised OK"));
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
/*  Motor helpers                                                         */
/* ===================================================================== */
/*
 * MOTOR_R_EN (GPIO 12) and MOTOR_L_EN (GPIO 4) are separate pins,
 * giving true independent PWM speed control per motor (differential drive).
 */

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  /* ESP32 Arduino Core 3.x — ledcAttach(pin, freq, res) */
  void setupPWM() {
      ledcAttach(MOTOR_R_EN, PWM_FREQ, PWM_RES);
      ledcAttach(MOTOR_L_EN, PWM_FREQ, PWM_RES);
  }
  void setMotorPWM(uint8_t pin, uint32_t duty) { ledcWrite(pin, duty); }
#else
  /* ESP32 Arduino Core 2.x */
  #define CH_MOTOR_R  2   // LEDC channel 2 (0 & 1 taken by camera XCLK)
  #define CH_MOTOR_L  3   // LEDC channel 3
  void setupPWM() {
      ledcSetup(CH_MOTOR_R, PWM_FREQ, PWM_RES);
      ledcAttachPin(MOTOR_R_EN, CH_MOTOR_R);
      ledcSetup(CH_MOTOR_L, PWM_FREQ, PWM_RES);
      ledcAttachPin(MOTOR_L_EN, CH_MOTOR_L);
  }
  void setMotorPWM(uint8_t pin, uint32_t duty) {
      ledcWrite(pin == MOTOR_R_EN ? CH_MOTOR_R : CH_MOTOR_L, duty);
  }
#endif

void setMotor(uint8_t en, uint8_t in1, uint8_t in2, int speed) {
    digitalWrite(in1, speed > 0 ? HIGH : LOW);
    digitalWrite(in2, speed < 0 ? HIGH : LOW);
    setMotorPWM(en, constrain(abs(speed), 0, 255));
}

void driveMotors(int left, int right) {
    setMotor(MOTOR_R_EN, MOTOR_R_IN1, MOTOR_R_IN2, right);
    setMotor(MOTOR_L_EN, MOTOR_L_IN1, MOTOR_L_IN2, left);
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
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);
    setupPWM();

    /* ── Camera ─────────────────────────────────────────────────────── */
    if (!initCamera()) {
        Serial.println(F("!! Camera init failed — continuing without video."));
    }

    /* ── WiFi Access Point ──────────────────────────────────────────── */
    if (!robolink.beginAP(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println(F("WiFi AP failed!"));
        while (true) delay(1000);
    }

    Serial.print(F("WiFi AP  : ")); Serial.println(WIFI_SSID);
    Serial.print(F("IP       : ")); Serial.println(robolink.localIP());
    Serial.print(F("UDP port : ")); Serial.println(robolink.port());
    Serial.printf( "Stream   : http://%s:%d/stream\n",
                    robolink.localIP().toString().c_str(), STREAM_PORT);

    /* ── MJPEG stream task on Core 0 ─────────────────────────────────── */
    xTaskCreatePinnedToCore(
        streamTask,       /* task function      */
        "mjpeg",          /* name               */
        4096,             /* stack size (bytes) */
        NULL,             /* parameter          */
        1,                /* priority           */
        NULL,             /* task handle        */
        0                 /* Core 0             */
    );

    /* ── RoboLink settings ──────────────────────────────────────────── */
    robolink.setTimeoutMs(500);
    robolink.setSendInterval(200);

    Serial.println(F("\n── App widgets ──────────────────────────"));
    Serial.println(F("  Joystick  → keys \"joy_x\", \"joy_y\""));
    Serial.printf( "  Camera    → URL  http://%s:%d/stream\n",
                    robolink.localIP().toString().c_str(), STREAM_PORT);
    Serial.println(F("─────────────────────────────────────────\n"));
}

/* ===================================================================== */
/*  loop()                                                                */
/* ===================================================================== */
void loop() {
    /* ── Handle RoboLink UDP control data ───────────────────────────── */
    robolink.update();

    /* (MJPEG stream runs on Core 0 — nothing to do here) */

    /* ═══════════════════════════════════════════════════════════════════
     *  Differential Drive
     * ═══════════════════════════════════════════════════════════════════
     *  throttle (joy_y) : base speed for both motors  –255 … +255
     *  steer    (joy_x) : speed *difference*           –255 … +255
     *
     *  left  = throttle + steer
     *  right = throttle – steer
     *
     *  Steer > 0 (right) → deducts from right, adds to left.
     *  Steer < 0 (left)  → deducts from left,  adds to right.
     *
     *  When throttle == 0 and steer != 0 the car pivots in place.
     *
     *  Instead of hard-clamping (which wrecks the left/right ratio at
     *  the extremes), we proportionally scale both motors so the
     *  steering feel stays consistent at every speed.
     */

    int rawThrottle = robolink.get("joy_y");   // –100 … +100
    int rawSteer    = robolink.get("joy_x");   // –100 … +100

    /* Map joystick range (–100..100) to full PWM range (–255..255) */
    int throttle = map(rawThrottle, -100, 100, -255, 255);
    int steer    = map(rawSteer,    -100, 100, -255, 255);

    /* Small dead-zone to stop micro-drift when sticks are centred */
    if (abs(throttle) < 10) throttle = 0;
    if (abs(steer)    < 10) steer    = 0;

    int left  = throttle + steer;
    int right = throttle - steer;

    /*
     * Proportional scaling — keeps the L/R ratio intact.
     * Example: throttle 200, steer 200 → raw L=400, R=0
     *          scale factor = 255/400  → L=255, R=0  (ratio preserved)
     * Clamping would give L=255, R=0 too in this case, but at diagonal
     * extremes clamping distorts the curve while scaling does not.
     */
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
    driveMotors(left, right);

    /* ── Send sensor data back to the app ───────────────────────────── */
    robolink.setSensor("speed_l", speedL);
    robolink.setSensor("speed_r", speedR);
    robolink.setSensor("uptime",  (int)(millis() / 1000));

    /* ── Periodic status to Serial Monitor ──────────────────────────── */
    if (millis() - lastPrint >= 3000) {
        lastPrint = millis();
        if (robolink.clientCount() == 0) {
            Serial.println(F("[WiFi] No phone connected."));
        } else if (!robolink.hasReceivedData()) {
            Serial.println(F("[WiFi] Phone connected. Waiting for joystick..."));
        } else if (robolink.isTimedOut()) {
            Serial.println(F("[WiFi] Signal lost — motors stopped."));
        } else {
            Serial.printf("[OK] L=%d  R=%d\n", speedL, speedR);
        }
    }
}
