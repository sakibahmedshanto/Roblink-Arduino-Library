/*
 * RoboLinkWiFi.cpp  —  v2.0
 * ──────────────────────────
 * ESP32 WiFi (UDP) transport.  Callbacks fire AFTER reading.
 */

#if defined(ESP32)

#include "RoboLinkWiFi.h"
#include <string.h>

RoboLinkWiFi::RoboLinkWiFi()
    : _port(ROBOLINK_DEFAULT_PORT), _started(false), _apMode(false),
      _remotePort(0), _hasRemote(false),
      _valueCb(nullptr), _msgCb(nullptr),
      _sendIntervalMs(0), _lastSendMs(0)
{}

/* ════════════════════════════════════════════════════════════════════ */
/*  WiFi init                                                           */
/* ════════════════════════════════════════════════════════════════════ */

bool RoboLinkWiFi::beginAP(const char* ssid, const char* password,
                            uint16_t port) {
    _port = port; _apMode = true; _apSSID = ssid;
    WiFi.mode(WIFI_AP);
    bool ok = (password && strlen(password) > 0)
              ? WiFi.softAP(ssid, password) : WiFi.softAP(ssid);
    if (!ok) return false;
    delay(100);
    _started = (_udp.begin(_port) != 0);
    return _started;
}

bool RoboLinkWiFi::beginSTA(const char* ssid, const char* password,
                             uint16_t port, unsigned long timeoutMs) {
    _port = port; _apMode = false;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - t0 >= timeoutMs) return false;
        delay(250);
    }
    _started = (_udp.begin(_port) != 0);
    return _started;
}

bool RoboLinkWiFi::beginSaved(uint16_t port) {
    _prefs.begin("robolink", true);
    String s = _prefs.getString("ssid", "");
    String p = _prefs.getString("pass", "");
    _prefs.end();
    if (s.length() == 0) return false;
    return beginSTA(s.c_str(), p.c_str(), port);
}

void RoboLinkWiFi::stop() {
    _udp.stop(); WiFi.disconnect(true);
    _started = false; _hasRemote = false;
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Credentials                                                         */
/* ════════════════════════════════════════════════════════════════════ */

void RoboLinkWiFi::saveCredentials(const char* ssid, const char* password) {
    _prefs.begin("robolink", false);
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", password ? password : "");
    _prefs.end();
}

bool RoboLinkWiFi::hasSavedCredentials() {
    _prefs.begin("robolink", true);
    bool ok = _prefs.getString("ssid", "").length() > 0;
    _prefs.end();
    return ok;
}

void RoboLinkWiFi::clearCredentials() {
    _prefs.begin("robolink", false);
    _prefs.remove("ssid"); _prefs.remove("pass");
    _prefs.end();
}

bool RoboLinkWiFi::setupFromSerial(unsigned long timeoutMs,
                                    bool connectNow, uint16_t port) {
    Serial.println(F("\n=== RoboLink WiFi Setup ==="));
    Serial.println(F("Enter WiFi SSID:"));
    String ssid;
    unsigned long t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (Serial.available()) {
            ssid = Serial.readStringUntil('\n'); ssid.trim();
            if (ssid.length() > 0) break;
        }
        delay(10);
    }
    if (ssid.length() == 0) { Serial.println(F("Timeout.")); return false; }
    Serial.print(F("SSID: ")); Serial.println(ssid);

    Serial.println(F("Enter password (blank=open):"));
    String pass;
    t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (Serial.available()) {
            pass = Serial.readStringUntil('\n'); pass.trim(); break;
        }
        delay(10);
    }
    saveCredentials(ssid.c_str(), pass.c_str());
    Serial.println(F("Saved."));
    if (connectNow) {
        Serial.print(F("Connecting...")); 
        if (beginSTA(ssid.c_str(), pass.c_str(), port)) {
            Serial.print(F(" OK  IP: ")); Serial.println(localIP());
            return true;
        }
        Serial.println(F(" FAILED.")); return false;
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════════ */
/*  update                                                              */
/* ════════════════════════════════════════════════════════════════════ */

void RoboLinkWiFi::update() {
    if (!_started) return;

    /* 1. Read all UDP packets */
    bool gotMsg = false;
    int packetSize = _udp.parsePacket();
    while (packetSize > 0) {
        int len = _udp.read(_udpBuf, sizeof(_udpBuf) - 1);
        if (len > 0) {
            _remoteIP   = _udp.remoteIP();
            _remotePort = _udp.remotePort();
            _hasRemote  = true;
            if (_rx.feed(_udpBuf, (size_t)len) > 0) gotMsg = true;
        }
        packetSize = _udp.parsePacket();
    }

    /* 2. Callbacks AFTER reading — only changed keys */
    if (gotMsg) {
        if (_valueCb) {
            uint64_t mask = _rx.changedMask();
            for (int i = 0; i < _rx.count() && mask; i++) {
                if (mask & ((uint64_t)1 << i)) {
                    _valueCb(_rx.keyAt(i), _rx.valueAt(i));
                    mask &= ~((uint64_t)1 << i);
                }
            }
        }
        if (_msgCb) _msgCb();
    }

    /* 3. Auto-send */
    if (_sendIntervalMs > 0 && _hasRemote && _sensors.count() > 0) {
        if (millis() - _lastSendMs >= _sendIntervalMs) {
            _lastSendMs = millis();
            sendSensors();
        }
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Delegates                                                           */
/* ════════════════════════════════════════════════════════════════════ */

int         RoboLinkWiFi::get(const char* k, int d) const { return _rx.get(k, d); }
bool        RoboLinkWiFi::has(const char* k) const        { return _rx.has(k); }
int         RoboLinkWiFi::dataCount() const               { return _rx.count(); }
const char* RoboLinkWiFi::keyAt(int i) const              { return _rx.keyAt(i); }
int         RoboLinkWiFi::valueAt(int i) const            { return _rx.valueAt(i); }
void        RoboLinkWiFi::forEach(RoboLinkValueCb cb) const { _rx.forEach(cb); }
void        RoboLinkWiFi::printAll(Stream& out) const     { _rx.printAll(out); }

void RoboLinkWiFi::setSensor(const char* k, int v) { _sensors.set(k, v); }
bool RoboLinkWiFi::removeSensor(const char* k)     { return _sensors.remove(k); }
void RoboLinkWiFi::setSendInterval(unsigned long ms){ _sendIntervalMs = ms; }
int  RoboLinkWiFi::sensorCount() const              { return _sensors.count(); }

void RoboLinkWiFi::sendSensors() {
    if (!_started || !_hasRemote || _sensors.count() == 0) return;
    char buf[ROBOLINK_LINE_BUF];
    size_t len = _sensors.buildMessage(buf, sizeof(buf));
    if (len > 0) {
        _udp.beginPacket(_remoteIP, _remotePort);
        _udp.write((const uint8_t*)buf, len);
        _udp.endPacket();
    }
}

void RoboLinkWiFi::printSensors(Stream& out) const { _sensors.printAll(out); }

void RoboLinkWiFi::onReceive(RoboLinkValueCb cb) { _valueCb = cb; }
void RoboLinkWiFi::onMessage(RoboLinkMsgCb cb)   { _msgCb   = cb; }

void RoboLinkWiFi::setTimeoutMs(unsigned long ms) { _rx.setTimeoutMs(ms); }
bool RoboLinkWiFi::isTimedOut() const              { return _rx.isTimedOut(); }
bool RoboLinkWiFi::hasReceivedData() const         { return _rx.hasReceivedData(); }

bool      RoboLinkWiFi::isConnected() const  { return _apMode ? true : (WiFi.status() == WL_CONNECTED); }
bool      RoboLinkWiFi::isAPMode() const     { return _apMode; }
int       RoboLinkWiFi::clientCount() const  { return _apMode ? (int)WiFi.softAPgetStationNum() : (WiFi.status() == WL_CONNECTED ? 1 : 0); }
IPAddress RoboLinkWiFi::localIP() const      { return _apMode ? WiFi.softAPIP() : WiFi.localIP(); }
String    RoboLinkWiFi::ssid() const         { return _apMode ? _apSSID : WiFi.SSID(); }
uint16_t  RoboLinkWiFi::port() const         { return _port; }
IPAddress RoboLinkWiFi::remoteIP() const     { return _remoteIP; }

RoboLinkParser& RoboLinkWiFi::parser() { return _rx; }
WiFiUDP&        RoboLinkWiFi::udp()    { return _udp; }

#endif /* ESP32 */
