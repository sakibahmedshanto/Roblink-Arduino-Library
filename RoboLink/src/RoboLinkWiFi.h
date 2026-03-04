/*
 * RoboLinkWiFi.h  —  v2.0
 * ────────────────────────
 * ESP32 WiFi (UDP broadcast) transport.
 *
 * Platform: ESP32 only
 */

#ifndef ROBOLINK_WIFI_H
#define ROBOLINK_WIFI_H

#if defined(ESP32)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "RoboLinkParser.h"

#ifndef ROBOLINK_DEFAULT_PORT
#define ROBOLINK_DEFAULT_PORT 4210
#endif

class RoboLinkWiFi {
public:
    RoboLinkWiFi();

    /* ── WiFi init ───────────────────────────────────────────────────── */
    bool beginAP(const char* ssid, const char* password = nullptr,
                 uint16_t port = ROBOLINK_DEFAULT_PORT);
    bool beginSTA(const char* ssid, const char* password,
                  uint16_t port = ROBOLINK_DEFAULT_PORT,
                  unsigned long timeoutMs = 15000);
    bool beginSaved(uint16_t port = ROBOLINK_DEFAULT_PORT);
    void stop();

    /* ── credential management ───────────────────────────────────────── */
    void saveCredentials(const char* ssid, const char* password);
    bool hasSavedCredentials();
    void clearCredentials();
    bool setupFromSerial(unsigned long timeoutMs = 30000,
                         bool connectNow = true,
                         uint16_t port = ROBOLINK_DEFAULT_PORT);

    /* ── main loop ───────────────────────────────────────────────────── */
    void update();

    /* ── received data ───────────────────────────────────────────────── */
    int         get(const char* key, int defaultVal = 0) const;
    bool        has(const char* key) const;
    int         dataCount() const;
    const char* keyAt(int index) const;
    int         valueAt(int index) const;
    void        forEach(RoboLinkValueCb cb) const;
    void        printAll(Stream& out = Serial) const;

    /* ── outgoing sensors ────────────────────────────────────────────── */
    void setSensor(const char* key, int value);
    bool removeSensor(const char* key);
    void setSendInterval(unsigned long ms);
    void sendSensors();
    void printSensors(Stream& out = Serial) const;
    int  sensorCount() const;

    /* ── callbacks (fire AFTER packet read) ──────────────────────────── */
    void onReceive(RoboLinkValueCb cb);
    void onMessage(RoboLinkMsgCb cb);

    /* ── timeout ─────────────────────────────────────────────────────── */
    void setTimeoutMs(unsigned long ms);
    bool isTimedOut() const;
    bool hasReceivedData() const;

    /* ── status ──────────────────────────────────────────────────────── */
    bool      isConnected() const;
    bool      isAPMode() const;
    int       clientCount() const;
    IPAddress localIP() const;
    String    ssid() const;
    uint16_t  port() const;
    IPAddress remoteIP() const;

    /* ── advanced ────────────────────────────────────────────────────── */
    RoboLinkParser& parser();
    WiFiUDP&        udp();

private:
    RoboLinkParser _rx;
    RoboLinkParser _sensors;

    WiFiUDP     _udp;
    uint16_t    _port;
    bool        _started;
    bool        _apMode;

    IPAddress   _remoteIP;
    uint16_t    _remotePort;
    bool        _hasRemote;

    Preferences _prefs;
    uint8_t     _udpBuf[ROBOLINK_LINE_BUF];
    String      _apSSID;

    RoboLinkValueCb _valueCb;
    RoboLinkMsgCb   _msgCb;

    unsigned long _sendIntervalMs;
    unsigned long _lastSendMs;
};

#endif /* ESP32 */
#endif
