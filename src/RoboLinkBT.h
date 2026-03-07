/*
 * RoboLinkBT.h  —  v2.0
 * ──────────────────────
 * ESP32 Bluetooth Classic (SPP) transport.
 * Same deferred-callback pattern as RoboLinkSerial.
 *
 * Platform: ESP32 only (Classic BT enabled in sdkconfig)
 */

#ifndef ROBOLINK_BT_H
#define ROBOLINK_BT_H

#if defined(ESP32)

#include <Arduino.h>
#include "BluetoothSerial.h"
#include "RoboLinkParser.h"

class RoboLinkBT {
public:
    RoboLinkBT();

    bool begin(const char* deviceName = "RoboLink");
    void end();

    /** Read BT data, parse, fire callbacks, auto-send sensors. */
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

    /* ── callbacks (fire AFTER all bytes read) ───────────────────────── */
    void onReceive(RoboLinkValueCb cb);
    void onMessage(RoboLinkMsgCb cb);

    /* ── timeout ─────────────────────────────────────────────────────── */
    void setTimeoutMs(unsigned long ms);
    bool isTimedOut() const;
    bool hasReceivedData() const;

    /* ── status ──────────────────────────────────────────────────────── */
    bool isStarted() const;
    bool hasClient() const;

    /* ── advanced ────────────────────────────────────────────────────── */
    RoboLinkParser&  parser();
    BluetoothSerial& serial();

private:
    RoboLinkParser  _rx;
    RoboLinkParser  _sensors;
    BluetoothSerial _bt;
    bool            _started;

    RoboLinkValueCb _valueCb;
    RoboLinkMsgCb   _msgCb;

    unsigned long   _sendIntervalMs;
    unsigned long   _lastSendMs;
};

#endif /* ESP32 */
#endif
