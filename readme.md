# RoboLink Library — Documentation
**Version 2.0.0**

Connect your ESP32 (or Arduino) to the RoboLink mobile app over **WiFi**, **Bluetooth**, or **Serial**.  
Send joystick, button, and slider values from your phone to your robot. Send sensor data back.

---

## Table of Contents

1. [Installation](#1-installation)
2. [Architecture Overview](#2-architecture-overview)
3. [Wire Protocol](#3-wire-protocol)
4. [Classes](#4-classes)
   - [RoboLinkWiFi](#41-robolinkwifi)
   - [RoboLinkBT](#42-robolinkbt)
   - [RoboLinkSerial](#43-robolinkserial)
   - [RoboLinkParser](#44-robolinkparser)
5. [App Widget Reference](#5-app-widget-reference)
6. [Sending Sensor Data](#6-sending-sensor-data)
7. [Timeout & Safety Stop](#7-timeout--safety-stop)
8. [Callbacks](#8-callbacks)
9. [WiFi Credential Storage](#9-wifi-credential-storage)
10. [Configuration & Limits](#10-configuration--limits)
11. [Examples](#11-examples)
12. [Troubleshooting](#12-troubleshooting)

---

## 1. Installation

### Arduino IDE (manual)

1. Copy the `RoboLink/` folder into your Arduino libraries directory:
   - **Windows:** `Documents\Arduino\libraries\RoboLink\`
   - **macOS / Linux:** `~/Arduino/libraries/RoboLink/`
2. Restart the Arduino IDE.
3. Open **Sketch → Include Library → RoboLink**.

### PlatformIO

```ini
lib_deps =
    https://github.com/nicezki/robolink.git
```

Or copy the `RoboLink/` folder into your project's `lib/` directory.

---

## 2. Architecture Overview

```
┌──────────────────────────────────────────────┐
│                 #include <RoboLink.h>         │
├──────────────┬──────────────┬────────────────┤
│ RoboLinkWiFi │  RoboLinkBT  │ RoboLinkSerial │
│  (ESP32 only)│  (ESP32 only)│  (all boards)  │
├──────────────┴──────────────┴────────────────┤
│              RoboLinkParser                   │
│   (zero-heap, AVR-safe, internal engine)      │
└──────────────────────────────────────────────┘
```

| Transport | Platform | Connection |
|-----------|----------|------------|
| `RoboLinkWiFi` | ESP32 | UDP over WiFi (AP or Station mode) |
| `RoboLinkBT` | ESP32 | Bluetooth Classic SPP |
| `RoboLinkSerial` | All (Arduino, ESP32, etc.) | Serial / SoftwareSerial / HC-05 |

All three transports expose **identical data-access methods** (`get`, `has`, `setSensor`, etc.) so you can swap transports without rewriting your control logic.

---

## 3. Wire Protocol

Both directions (app → device and device → app) use the same simple ASCII format:

```
key1:val1,key2:val2,...,keyN:valN\n
```

- **Keys** — ASCII strings (letters, digits, underscores)
- **Values** — signed 32-bit integers
- **Delimiter** — comma between pairs
- **Terminator** — newline `\n`

### Examples

**App → Device (controls):**
```
throttle:80,steer:-45,speed:200,horn:0\n
```

**Device → App (sensors):**
```
temperature:24,battery:87,distance:120,uptime:43\n
```

The app transmits at **20 Hz** (every 50 ms) by default.

---

## 4. Classes

### 4.1 RoboLinkWiFi

ESP32 WiFi transport using UDP on port **4210** (default).

#### Minimal example

```cpp
#include <RoboLink.h>

RoboLinkWiFi robolink;

void setup() {
    robolink.beginAP("MyRobot", "12345678");
    robolink.setTimeoutMs(500);
}

void loop() {
    robolink.update();                         // must call every loop!

    if (robolink.isTimedOut()) { /* stop motors */ return; }

    int throttle = robolink.get("throttle");   // -100 to +100
    int steer    = robolink.get("steer");      // -100 to +100
}
```

#### Initialisation methods

| Method | Description |
|--------|-------------|
| `beginAP(ssid, password, port)` | Create a WiFi Access Point. Phone connects directly to the ESP32. |
| `beginSTA(ssid, password, port, timeout)` | Join an existing WiFi network (both phone and ESP32 on same network). |
| `beginSaved(port)` | Connect using credentials saved in NVS (see [Section 9](#9-wifi-credential-storage)). |
| `stop()` | Shut down WiFi and stop UDP listener. |

#### Main loop

| Method | Description |
|--------|-------------|
| `update()` | **Call every `loop()` iteration.** Reads incoming UDP packets, parses data, fires callbacks, auto-sends sensors. |

#### Reading received data

| Method | Description |
|--------|-------------|
| `get(key)` | Returns the latest value for `key` (0 if not received). |
| `get(key, default)` | Returns `default` if key has never been received. |
| `has(key)` | Returns `true` if key exists in the last packet. |
| `dataCount()` | Number of key-value pairs in the last packet. |
| `keyAt(i)` | Key name at index `i`. |
| `valueAt(i)` | Value at index `i`. |
| `forEach(callback)` | Calls `callback(key, value)` for every pair. |
| `printAll(stream)` | Prints all pairs to `stream` (default `Serial`). |

#### Sending sensor data

| Method | Description |
|--------|-------------|
| `setSensor(key, value)` | Store a sensor value to be sent. |
| `removeSensor(key)` | Remove a sensor key. |
| `setSendInterval(ms)` | Auto-send sensors every `ms` milliseconds (set in `setup()`). |
| `sendSensors()` | Immediately transmit all stored sensor values. |
| `printSensors(stream)` | Print outgoing sensor map to `stream`. |
| `sensorCount()` | Number of sensor keys stored. |

#### Timeout

| Method | Description |
|--------|-------------|
| `setTimeoutMs(ms)` | Mark as timed-out if no packet in `ms` milliseconds (0 = disabled). |
| `isTimedOut()` | `true` if no data received for longer than the timeout. |
| `hasReceivedData()` | `true` if at least one packet has ever been received. |

#### Callbacks

| Method | Description |
|--------|-------------|
| `onReceive(cb)` | `void cb(const char* key, int value)` — called after each complete packet. |
| `onMessage(cb)` | `void cb()` — called on every complete packet. |

#### Status

| Method | Returns |
|--------|---------|
| `isConnected()` | `true` if WiFi is up. |
| `isAPMode()` | `true` if running as Access Point. |
| `clientCount()` | Number of clients connected to the AP (AP mode only). |
| `localIP()` | ESP32's IP address (`IPAddress`). |
| `ssid()` | Current SSID (`String`). |
| `port()` | UDP port number in use. |
| `remoteIP()` | IP of the last app that sent data. |

#### WiFi credential management

| Method | Description |
|--------|-------------|
| `saveCredentials(ssid, pass)` | Save SSID + password to ESP32 NVS flash. |
| `hasSavedCredentials()` | Returns `true` if credentials are stored. |
| `clearCredentials()` | Erase saved credentials from NVS. |
| `setupFromSerial(timeout, connect, port)` | Interactive setup via Serial Monitor — prompts for SSID and password. |

---

### 4.2 RoboLinkBT

ESP32 Bluetooth Classic (SPP) transport.

#### Minimal example

```cpp
#include <RoboLink.h>

RoboLinkBT robolink;

void setup() {
    robolink.begin("MyRobot_BT");
    robolink.setTimeoutMs(500);
}

void loop() {
    robolink.update();

    if (!robolink.hasClient() || robolink.isTimedOut()) return;

    int throttle = robolink.get("throttle");
    int steer    = robolink.get("steer");
}
```

**Phone setup:** Settings → Bluetooth → Pair "MyRobot_BT" → open RoboLink app → select Bluetooth.

#### Methods

| Method | Description |
|--------|-------------|
| `begin(deviceName)` | Start Bluetooth with the given device name. |
| `end()` | Stop Bluetooth. |
| `update()` | **Must call every loop.** Reads, parses, fires callbacks. |
| `get(key, default)` | Read a received control value. |
| `has(key)` | Check if key was received. |
| `setSensor(key, value)` | Set outgoing sensor value. |
| `sendSensors()` | Transmit sensor data. |
| `setSendInterval(ms)` | Auto-send sensors periodically. |
| `onReceive(cb)` / `onMessage(cb)` | Register callbacks. |
| `setTimeoutMs(ms)` / `isTimedOut()` | Timeout detection. |
| `isStarted()` | `true` if `begin()` succeeded. |
| `hasClient()` | `true` if a phone is currently connected. |
| `parser()` | Access the internal `RoboLinkParser`. |
| `serial()` | Access the internal `BluetoothSerial` object. |

---

### 4.3 RoboLinkSerial

Works on **all boards** (Arduino Uno, Mega, ESP32, etc.) with any `Stream` — `Serial`, `Serial1`, `SoftwareSerial`, HC-05/HC-06, etc.

#### Minimal example

```cpp
#include <RoboLink.h>
#include <SoftwareSerial.h>

SoftwareSerial btSerial(10, 11);   // RX, TX
RoboLinkSerial robolink;

void setup() {
    Serial.begin(115200);
    btSerial.begin(9600);
    robolink.begin(btSerial);      // attach stream
    robolink.setTimeoutMs(500);
}

void loop() {
    robolink.update();

    int throttle = robolink.get("throttle");
    int steer    = robolink.get("steer");
}
```

#### Methods

| Method | Description |
|--------|-------------|
| `begin(stream)` | Attach a single `Stream` for both RX and TX. |
| `begin(input, output*)` | Separate RX/TX streams. Pass `nullptr` for output to disable sending (recommended for SoftwareSerial). |
| `end()` | Detach streams. |
| `update()` | **Must call every loop.** |
| `get(key, default)` | Read received value. |
| `has(key)` | Check key presence. |
| `setSensor(key, value)` | Set outgoing sensor value. |
| `sendSensors()` | Transmit sensor data. |
| `setSendInterval(ms)` | Auto-send sensors periodically. |
| `onReceive(cb)` / `onMessage(cb)` | Register callbacks. |
| `setTimeoutMs(ms)` / `isTimedOut()` | Timeout detection. |
| `isStarted()` | `true` if `begin()` was called. |
| `bytesReceived()` | Total bytes received since `begin()`. |
| `messagesReceived()` | Total complete messages received. |

---

### 4.4 RoboLinkParser

The low-level parser used internally by all transports. You can also use it standalone to parse RoboLink messages from any data source.

```cpp
RoboLinkParser parser;

// Feed bytes one at a time
if (parser.feed(byte)) {
    // A complete message was parsed
    int val = parser.get("throttle");
}

// Or feed a buffer
parser.feed(buf, len);
```

#### Methods

| Method | Description |
|--------|-------------|
| `feed(byte)` | Feed one byte. Returns `true` when a complete message is parsed. |
| `feed(data, len)` | Feed a block. Returns number of complete messages parsed. |
| `get(key, default)` | Get value by key name. |
| `has(key)` | Check if key exists. |
| `count()` | Number of pairs stored. |
| `keyAt(i)` / `valueAt(i)` | Access by index. |
| `set(key, value)` | Manually insert a key-value pair. |
| `remove(key)` | Remove a key. |
| `forEach(cb)` | Iterate all pairs. |
| `printAll(stream)` | Print all pairs to a stream. |
| `buildMessage(buf, size)` | Serialize stored pairs into a wire-format string. |
| `buildMessage()` | Returns wire-format `String` (ESP32/ARM only). |
| `setTimeoutMs(ms)` | Configure timeout. |
| `isTimedOut()` / `hasReceivedData()` | Timeout state. |
| `lastRxTime()` | `millis()` timestamp of last received message. |
| `changed(index)` | Was the value at this index updated in the last packet? |
| `changedMask()` | Bitmask of all changed keys (up to 64). |
| `changedCount()` | Number of keys that changed. |
| `clear()` | Reset all stored data. |

---

## 5. App Widget Reference

Configure widgets in the RoboLink app with these key names:

| Widget | Keys sent by app | Value range | Behaviour |
|--------|-----------------|-------------|-----------|
| **Joystick** | `xKey`, `yKey` (configurable) | min – max (default center = 0) | Springs back to center on release |
| **Button** | `dataKey` | `onValue` (held) / `offValue` (released) default 1/0 | Momentary |
| **Toggle** | `dataKey` | `onValue` / `offValue` default 1/0 | Latching — tap to flip state |
| **Slider** | `dataKey` | min – max (default 0–255) | Stays where dragged |
| **Vertical Slider** | `dataKey` | min – max (default 0–180) | Vertical version of slider |
| **D-Pad** | `{key}_up`, `{key}_down`, `{key}_left`, `{key}_right` | onValue / 0 | 4 independent momentary buttons |
| **XY Pad** | `xKey`, `yKey` | min – max | 2D touch area, no spring |
| **Trigger** | `dataKey` | 0 – maxValue | Springs back to 0 on release |
| **Gauge** | *(display only)* | Reads from device sensor stream | Displays sensor values |
| **Camera** | *(display only)* | Reads MJPEG stream URL | Live video from ESP32-CAM |

### Built-in presets

| Preset | Keys |
|--------|------|
| **RC Car** | `steer`, `throttle`, `speed`, `horn`, `lights`, `brake`, `reverse` |
| **Robotic Arm** | `base`, `shoulder`, `elbow`, `wrist`, `grip`, `arm_x`, `arm_y` |
| **Drone** | `roll`, `pitch`, `yaw`, `throttle`, `speed`, `arm`, `photo`, `rtl` |

---

## 6. Sending Sensor Data

Send readings from your ESP32 back to be displayed in the app (Gauge widgets):

```cpp
void loop() {
    robolink.update();

    // Set values anytime — they are queued until sent
    robolink.setSensor("temperature", 24);
    robolink.setSensor("battery",     87);
    robolink.setSensor("distance",   120);
    robolink.setSensor("uptime", (int)(millis() / 1000));

    // Option A: send at a fixed interval (set once in setup)
    //   robolink.setSendInterval(200);   // auto-send every 200 ms

    // Option B: send manually at your own rate
    robolink.sendSensors();   // call at 5–10 Hz max
}
```

> **Tip:** Do not call `sendSensors()` every loop iteration — 5–10 Hz is plenty and avoids flooding the UDP/BT link.

---

## 7. Timeout & Safety Stop

Always configure a timeout so motors stop automatically if the phone disconnects:

```cpp
void setup() {
    robolink.beginAP("MyRobot", "12345678");
    robolink.setTimeoutMs(500);    // 500 ms with no data = timed out
}

void loop() {
    robolink.update();

    if (robolink.isTimedOut() || !robolink.hasReceivedData()) {
        stopAllMotors();
        return;
    }

    // Safe to read controls here
    int throttle = robolink.get("throttle");
}
```

| State | `isTimedOut()` | `hasReceivedData()` | Meaning |
|-------|---------------|---------------------|---------|
| Just powered on | `false` | `false` | No phone has ever connected |
| Phone connected, sending | `false` | `true` | Normal operation |
| Phone disconnected | `true` | `true` | Signal lost — stop motors! |

---

## 8. Callbacks

Callbacks are executed once per **complete packet**, after all bytes in that packet are read (SoftwareSerial-safe).

```cpp
// Called for every key-value pair in each incoming packet
robolink.onReceive([](const char* key, int value) {
    Serial.print(key); Serial.print(" = "); Serial.println(value);
});

// Called once per complete incoming packet
robolink.onMessage([]() {
    Serial.println("New packet received!");
});
```

Both callbacks are optional. You can use them alongside `get()` — they are not mutually exclusive.

---

## 9. WiFi Credential Storage

Credentials are saved to ESP32 **NVS** (flash) and survive reboots and re-flashing.

```cpp
// ── First run: save credentials ──────────────────────────────────────
robolink.saveCredentials("MyHomeWiFi", "password123");

// ── All subsequent runs: connect automatically ────────────────────────
if (robolink.hasSavedCredentials()) {
    robolink.beginSaved();
} else {
    // Fall back to AP mode
    robolink.beginAP("RoboLink", "12345678");
}
```

### Interactive serial setup

Prompt for SSID and password via Serial Monitor (useful for first-time deployment):

```cpp
// In setup() — waits up to 30 seconds for input, then connects
robolink.setupFromSerial(30000, true);
```

### Clear credentials

```cpp
robolink.clearCredentials();
```

---

## 10. Configuration & Limits

Override these **before** `#include <RoboLink.h>` to customize memory usage:

```cpp
#define ROBOLINK_MAX_PAIRS    64    // max key-value pairs per packet (default: 64 on ESP32, 8 on AVR)
#define ROBOLINK_MAX_KEY_LEN  32    // max characters per key name (default: 24 on ESP32, 12 on AVR)
#define ROBOLINK_LINE_BUF    1024   // input line buffer in bytes (default: 512 on ESP32, 128 on AVR)
#define ROBOLINK_DEFAULT_PORT 5000  // UDP port (default: 4210)
#include <RoboLink.h>
```

### Memory footprint (ESP32, default settings)

| Component | RAM used |
|-----------|----------|
| RX parser (received data) | ~1.4 KB |
| TX parser (sensor data) | ~1.4 KB |
| UDP receive buffer | ~512 B |
| **Total per instance** | **~3.3 KB** |

ESP32 has ~320 KB SRAM — this library uses well under 2% of it.

---

## 11. Examples

All examples are in the `examples/` folder.

| Example | Transport | What it does |
|---------|-----------|--------------|
| `Arduino_PrintAllData` | Serial (HC-05) | Prints every received key-value pair |
| `Arduino_RC_Car` | Serial (HC-05) | 2-motor RC car on Arduino with L298N |
| `Arduino_BT_NoLibrary` | Serial | Raw parsing without the library |
| `Esp32_PrintAllData_Wifi` | WiFi AP | Prints all received data over WiFi |
| `ESP32_RC_Car_Wifi` | WiFi AP | Simple 2-motor RC car on ESP32 |
| `ESP32_CAM_RC_Car` | WiFi AP + Camera | 4-motor car with MJPEG camera stream |
| `ESP32_CAM_RC_Car` | WiFi AP + Camera | 4-wheel RC car on ESP32-CAM, live video |
| `esp32_SensorData_Wifi` | WiFi AP | Send temperature, distance, battery to app |
| `WiFiSetup` | WiFi STA | Credential manager — save and connect |
| `ESP32_WiFi_NoLibrary` | WiFi | Raw parsing without the library |

---

## 12. Troubleshooting

### No data arriving

| Check | Fix |
|-------|-----|
| WiFi AP mode: phone connected to ESP32's network? | Check SSID/password in code vs phone WiFi settings |
| WiFi STA mode: same network? | Confirm phone and ESP32 are on identical SSID |
| Port mismatch? | Default is **4210** — must match app and `beginAP()` / `beginSTA()` |
| `update()` not called? | Add `robolink.update()` at the top of `loop()` |
| `delay()` blocking? | Replace long `delay()` with `millis()`-based timing |

### Values always 0

- Key name mismatch — compare the key string in your code with the widget's **data key** setting in the app exactly (case-sensitive).
- Call `robolink.printAll()` in `loop()` to see what keys are actually arriving.

### Bluetooth won't start

- Requires **Bluetooth Classic** enabled. In PlatformIO add:
  ```ini
  build_flags = -DCONFIG_BT_CLASSIC_ENABLED
  ```
- In Arduino IDE, select **ESP32 Dev Module** board.

### Compile errors on Arduino Uno/Mega

- `RoboLinkWiFi` and `RoboLinkBT` are ESP32-only. Use `RoboLinkSerial` with an HC-05/HC-06 module for non-ESP32 boards.

### Serial Monitor shows garbage

- Set baud rate to **115200** to match `Serial.begin(115200)`.

### ESP32-CAM: camera and motor conflict on GPIO 4

- GPIO 4 is the camera flash LED on AI-Thinker boards and should **not** be used for motor control.
- Use **GPIO 12** as a single speed pin (tie ENA + ENB on L298N together) to free up GPIO 4 for other uses.

---

*RoboLink v2.0.0 — MIT License*
