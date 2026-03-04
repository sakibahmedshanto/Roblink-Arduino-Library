# RoboLink Arduino Library

**ESP32 companion library for the [RoboLink](https://github.com/nicezki/robolink) controller app.**

Receive real-time control data from the RoboLink Flutter app over **WiFi (UDP)** or **Bluetooth Classic (SPP)** with a dead-simple API. Send sensor data back just as easily.

```cpp
#include <RoboLink.h>
RoboLinkWiFi robolink;

void setup() {
    robolink.beginAP("MyRobot", "12345678");
}

void loop() {
    robolink.update();
    int steer    = robolink.get("steer");
    int throttle = robolink.get("throttle");
}
```

That's it. No protocol parsing, no packet handling, no boilerplate.

---

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Quick Start — WiFi](#quick-start--wifi)
- [Quick Start — Bluetooth](#quick-start--bluetooth)
- [API Reference](#api-reference)
  - [RoboLinkParser](#robolinkparser)
  - [RoboLinkWiFi](#robolinkwifi)
  - [RoboLinkBT](#robolinkbt)
- [Wire Protocol](#wire-protocol)
- [App Widget Reference](#app-widget-reference)
- [Sending Sensor Data](#sending-sensor-data)
- [WiFi Credential Management](#wifi-credential-management)
- [Configuration & Limits](#configuration--limits)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Features

| Feature | Description |
|---------|-------------|
| **Map-like API** | `get("key")` / `has("key")` / `forEach(callback)` |
| **WiFi (UDP)** | Access Point or Station mode, UDP broadcast on port 4210 |
| **Bluetooth Classic** | SPP serial — just pair and go |
| **Sensor data** | `setSensor("temp", 42)` + `sendSensors()` — back to the app |
| **Callbacks** | `onReceive(cb)` for value changes, `onMessage(cb)` per packet |
| **Timeout detection** | `isTimedOut()` — safety-stop your robot if the app disconnects |
| **WiFi credential storage** | Save/load SSID+password to ESP32 NVS via `saveCredentials()` |
| **Serial WiFi setup** | Interactive `setupFromSerial()` — type SSID & password in Serial Monitor |
| **Print helpers** | `printAll()` / `printSensors()` — debug in one line |
| **Zero dependencies** | Only uses ESP32 Arduino Core built-in libraries |
| **Configurable** | Adjust max pairs, key length, buffer size, port number |

---

## Installation

### Arduino IDE (manual)

1. Download or clone this folder: `RoboLink/`
2. Copy it into your Arduino libraries folder:
   - **Windows:** `Documents\Arduino\libraries\RoboLink\`
   - **macOS:** `~/Documents/Arduino/libraries/RoboLink/`
   - **Linux:** `~/Arduino/libraries/RoboLink/`
3. Restart the Arduino IDE.
4. Go to **Sketch → Include Library** — you should see **RoboLink**.

### PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
    https://github.com/nicezki/robolink.git
```

Or copy the `RoboLink/` folder into your project's `lib/` directory.

---

## Quick Start — WiFi

The ESP32 creates a WiFi Access Point. Your phone connects to it.

```cpp
#include <RoboLink.h>

RoboLinkWiFi robolink;

void setup() {
    Serial.begin(115200);

    // Create WiFi AP — phone connects to "MyRobot" network
    if (robolink.beginAP("MyRobot", "12345678")) {
        Serial.print("AP ready, IP: ");
        Serial.println(robolink.localIP());
    }

    // Safety timeout — stop motors if app disconnects
    robolink.setTimeoutMs(500);
}

void loop() {
    robolink.update();           // ← MUST call every loop!

    if (robolink.isTimedOut()) {
        // Stop motors here
        return;
    }

    int steer    = robolink.get("steer", 0);      // -255 to 255
    int throttle = robolink.get("throttle", 0);    // -255 to 255
    int speed    = robolink.get("speed", 128);     // 0 to 255
    bool horn    = robolink.get("horn") != 0;      // momentary
    bool lights  = robolink.get("lights") != 0;    // toggle

    // ... drive your motors ...
}
```

### Station Mode (join existing network)

```cpp
if (robolink.beginSTA("MyHomeWiFi", "password123")) {
    Serial.print("Connected! IP: ");
    Serial.println(robolink.localIP());
}
```

Both the phone and the ESP32 must be on the **same WiFi network**.

---

## Quick Start — Bluetooth

```cpp
#include <RoboLink.h>

RoboLinkBT robolink;

void setup() {
    Serial.begin(115200);
    robolink.begin("MyRobot_BT");    // Bluetooth device name
    robolink.setTimeoutMs(500);
}

void loop() {
    robolink.update();

    if (!robolink.hasClient() || robolink.isTimedOut()) {
        // No phone connected — stop motors
        return;
    }

    int steer    = robolink.get("steer");
    int throttle = robolink.get("throttle");
    // ...
}
```

**Phone setup:** Settings → Bluetooth → Pair "MyRobot_BT" → open RoboLink app → select Bluetooth mode.

---

## API Reference

### RoboLinkParser

The low-level parser. Used internally by `RoboLinkWiFi` and `RoboLinkBT`. You can also use it standalone with any `Stream` source.

| Method | Description |
|--------|-------------|
| `feed(uint8_t c)` | Feed one byte to the parser |
| `feed(data, len)` | Feed a byte/char buffer |
| `get(key, default)` | Get value for a key (returns default if missing) |
| `has(key)` | Check if key exists |
| `count()` | Number of stored pairs |
| `keyAt(i)` | Key name at index i |
| `valueAt(i)` | Value at index i |
| `set(key, value)` | Manually set a key (for outgoing data) |
| `remove(key)` | Remove a key |
| `forEach(callback)` | Call `callback(key, value)` for each pair |
| `printAll(stream)` | Print all pairs to a stream (default: Serial) |
| `buildMessage(buf, size)` | Build wire-format string into a char buffer |
| `buildMessage()` | Build wire-format string as Arduino `String` |
| `onValueChange(cb)` | Register callback for value changes |
| `onMessage(cb)` | Register callback for complete messages |
| `setTimeoutMs(ms)` | Set receive timeout (0 = disabled) |
| `isTimedOut()` | True if no data after timeout |
| `hasReceivedData()` | True if at least one message received |
| `lastRxTime()` | `millis()` timestamp of last message |
| `clear()` | Reset all stored data |

### RoboLinkWiFi

Full WiFi transport with UDP. Inherits all read methods from parser.

| Method | Description |
|--------|-------------|
| **Initialisation** | |
| `beginAP(ssid, password, port)` | Start as WiFi Access Point |
| `beginSTA(ssid, password, port, timeout)` | Connect to existing WiFi network |
| `beginSaved(port)` | Connect using saved NVS credentials |
| `stop()` | Disconnect WiFi and stop UDP |
| **Main Loop** | |
| `update()` | **Must call in `loop()`** — reads & parses UDP |
| **Received Data** | |
| `get(key, default)` | Read a control value |
| `has(key)` | Check if key received |
| `dataCount()` | Number of received key-value pairs |
| `keyAt(i)` / `valueAt(i)` | Access by index |
| `forEach(callback)` | Iterate all pairs |
| `printAll(stream)` | Dump all data to serial |
| **Sensor Data** | |
| `setSensor(key, value)` | Set an outgoing sensor value |
| `removeSensor(key)` | Remove a sensor key |
| `sendSensors()` | Transmit all sensor values via UDP |
| `printSensors(stream)` | Print outgoing sensor map |
| `sensorCount()` | Number of sensor keys |
| **Callbacks** | |
| `onReceive(cb)` | Called when a value changes: `void cb(key, value)` |
| `onMessage(cb)` | Called on complete message: `void cb()` |
| **Timeout** | |
| `setTimeoutMs(ms)` | Set receive timeout |
| `isTimedOut()` | No data for longer than timeout? |
| `hasReceivedData()` | At least one message received? |
| **WiFi Credentials** | |
| `saveCredentials(ssid, pass)` | Save to ESP32 NVS |
| `hasSavedCredentials()` | Credentials stored? |
| `clearCredentials()` | Erase from NVS |
| `setupFromSerial(timeout, connect, port)` | Interactive serial WiFi setup |
| **Status** | |
| `isConnected()` | WiFi up? |
| `localIP()` | ESP32's IP address |
| `ssid()` | Connected/AP SSID |
| `port()` | UDP port number |
| `remoteIP()` | IP of the app (last sender) |
| **Advanced** | |
| `parser()` | Access the internal `RoboLinkParser` |
| `udp()` | Access the internal `WiFiUDP` object |

### RoboLinkBT

Bluetooth Classic SPP transport. Same data-access API as WiFi.

| Method | Description |
|--------|-------------|
| `begin(deviceName)` | Start Bluetooth with given name |
| `end()` | Stop Bluetooth |
| `update()` | **Must call in `loop()`** |
| `get(key, default)` | Read a control value |
| `has(key)` / `dataCount()` / `keyAt(i)` / `valueAt(i)` | Data access |
| `forEach(cb)` / `printAll(stream)` | Iteration |
| `setSensor(key, value)` / `sendSensors()` | Send sensor data |
| `onReceive(cb)` / `onMessage(cb)` | Callbacks |
| `setTimeoutMs(ms)` / `isTimedOut()` | Timeout |
| `isStarted()` | `begin()` succeeded? |
| `hasClient()` | Phone currently connected? |
| `parser()` | Access internal parser |
| `serial()` | Access internal `BluetoothSerial` |

---

## Wire Protocol

Both WiFi and Bluetooth use the exact same ASCII protocol:

```
key1:val1,key2:val2,...,keyN:valN\n
```

- **Keys**: ASCII strings (letters, digits, underscores)
- **Values**: Signed 32-bit integers
- **Separator**: Comma between pairs
- **Terminator**: Newline (`\n`)
- **Direction**: Bidirectional (app → device for controls, device → app for sensors)

### Example messages

```
steer:0,throttle:128,speed:200,horn:0,lights:1\n
distance:42,battery:85,temperature:27\n
```

The app transmits at **20 Hz** by default (every 50 ms).

---

## App Widget Reference

These are the widget types in the RoboLink app and the keys they send:

| Widget | Keys | Range | Behaviour |
|--------|------|-------|-----------|
| **Joystick** | `xKey`, `yKey` | min–max (default 0, neutral=center) | Springs back to center |
| **Button** | `dataKey` | onValue / offValue (default 1/0) | Momentary (pressed/released) |
| **Toggle** | `dataKey` | onValue / offValue (default 1/0) | Latching (tap to flip) |
| **Slider** | `dataKey` | min–max (default 0–255) | Stays where dragged |
| **VSlider** | `dataKey` | min–max (default 0–180) | Vertical slider |
| **D-Pad** | `{dataKey}_up`, `_down`, `_left`, `_right` | onValue / 0 | 4 momentary buttons |
| **XY Pad** | `xKey`, `yKey` | min–max | 2D touch area, no spring |
| **Trigger** | `dataKey` | 0–maxValue | Springs back to 0 on release |
| **Gauge** | (read-only) | Displays a value from the data stream | Shows sensor data |
| **Camera** | (no data) | Displays MJPEG stream | URL in dataKey |

### Default presets

**RC Car:** `steer`, `throttle`, `speed`, `horn`, `lights`, `brake`, `reverse`

**Robotic Arm:** `base`, `shoulder`, `elbow`, `wrist`, `grip`, `arm_x`, `arm_y`

**Drone:** `roll`, `pitch`, `yaw`, `throttle`, `speed`, `arm`, `photo`, `rtl`

---

## Sending Sensor Data

Send readings from your ESP32 back to the app:

```cpp
// Set sensor values (call anytime)
robolink.setSensor("temperature", 42);
robolink.setSensor("battery", 85);
robolink.setSensor("distance", 120);

// Transmit all at once (call at your desired rate)
robolink.sendSensors();
```

The sensor data is sent using the **same wire protocol** (`key:val,key:val\n`) back to the app. Add **Gauge** widgets in the app with matching key names to display them.

### Tips

- Call `sendSensors()` at a moderate rate (5–10 Hz) to avoid flooding.
- WiFi: sensor data is sent as a UDP unicast to the phone's IP (auto-detected).
- Bluetooth: sensor data is sent over the SPP serial link.

---

## WiFi Credential Management

### Save & auto-connect

```cpp
// First boot — save credentials
robolink.saveCredentials("MyNetwork", "MyPassword");

// Subsequent boots — connect from saved
if (robolink.beginSaved()) {
    Serial.println("Connected!");
}
```

### Interactive serial setup

```cpp
// Prompts in Serial Monitor for SSID and password
robolink.setupFromSerial();
```

### Clear credentials

```cpp
robolink.clearCredentials();
```

Credentials are stored in ESP32 **NVS** (Non-Volatile Storage) and survive reboots, OTA updates, and re-flashing sketches (unless you erase the NVS partition).

---

## Configuration & Limits

Override these **before** including `<RoboLink.h>`:

```cpp
#define ROBOLINK_MAX_PAIRS   64    // default: 32 — max key-value pairs
#define ROBOLINK_MAX_KEY_LEN 32    // default: 24 — max characters per key
#define ROBOLINK_RX_BUFFER   1024  // default: 512 — line buffer size
#define ROBOLINK_DEFAULT_PORT 5000 // default: 4210 — UDP port
#include <RoboLink.h>
```

### Memory usage

With default settings, each transport class uses approximately:

| Component | RAM |
|-----------|-----|
| Parser (received data) | ~1.4 KB |
| Parser (sensor data) | ~1.4 KB |
| UDP buffer | ~512 B |
| **Total per instance** | **~3.3 KB** |

ESP32 has ~320 KB available SRAM — plenty of room.

---

## Examples

| Example | Transport | Description |
|---------|-----------|-------------|
| [PrintAllData](examples/PrintAllData/) | WiFi AP | Print every incoming key-value pair |
| [PrintAllData_BT](examples/PrintAllData_BT/) | Bluetooth | Same, but over Bluetooth |
| [ESP32_RC_Car](examples/ESP32_RC_Car/) | WiFi AP | Complete 2-motor car with L298N |
| [ESP32_RC_Car_BT](examples/ESP32_RC_Car_BT/) | Bluetooth | Same car, Bluetooth control |
| [SensorData](examples/SensorData/) | WiFi AP | Send temperature, analog, uptime, etc. |
| [WiFiSetup](examples/WiFiSetup/) | WiFi STA | Interactive credential manager |

---

## Troubleshooting

### No data received

1. **WiFi AP mode**: Is your phone connected to the ESP32's WiFi network? Check Serial Monitor for the AP name and password.
2. **WiFi STA mode**: Are both devices on the same network? Check the ESP32's IP.
3. **Bluetooth**: Is the device paired? Open phone Bluetooth settings first, pair, THEN connect from the app.
4. **Port mismatch**: The app defaults to port **4210**. Make sure your `beginAP()` / `beginSTA()` uses the same port.
5. **Firewall**: Some phones block UDP broadcast on public networks. Use AP mode for guaranteed connectivity.

### Values not updating

- Make sure you call `robolink.update()` in **every iteration** of `loop()`.
- Don't use `delay()` with values > 50 ms — it blocks `update()`.
- Check key names match exactly between the app layout and your code.

### Bluetooth won't start

- ESP32 must have **Bluetooth Classic** enabled. In PlatformIO, add to `platformio.ini`:
  ```ini
  build_flags = -DCONFIG_BT_CLASSIC_ENABLED
  ```
- In Arduino IDE, select "ESP32 Dev Module" and ensure Bluetooth is not disabled in board config.

### Compile errors on non-ESP32 boards

- `RoboLinkWiFi` and `RoboLinkBT` are **ESP32-only**. They are wrapped in `#if defined(ESP32)`.
- `RoboLinkParser` works on **any Arduino board** — include it directly with `#include "RoboLinkParser.h"`.

### Serial Monitor shows garbage

- Set baud rate to **115200** (matching `Serial.begin(115200)`).
- Set line ending to **Newline** for `setupFromSerial()`.

---

## License

MIT License — see [LICENSE](LICENSE) for details.
