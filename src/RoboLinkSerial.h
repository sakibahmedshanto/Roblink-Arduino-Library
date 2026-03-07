/*
 * RoboLinkSerial.h  —  v2.0
 * ──────────────────────────
 * Serial transport for Arduino + HC-05/HC-06  (also works on ESP32 UART).
 *
 * KEY DESIGN: update() drains ALL serial bytes FIRST, then fires
 * callbacks.  This prevents SoftwareSerial RX byte loss caused by
 * Serial.print() blocking inside callbacks.
 */

#ifndef ROBOLINK_SERIAL_H
#define ROBOLINK_SERIAL_H

#include <Arduino.h>
#include "RoboLinkParser.h"

class RoboLinkSerial {
public:
    RoboLinkSerial();

    /* ── init ────────────────────────────────────────────────────────── */

    /** Attach a single stream for both RX and TX. */
    void begin(Stream& stream);

    /**
     * Attach separate input/output streams.
     * Pass nullptr for output to disable sensor sends
     * (recommended for SoftwareSerial on AVR).
     */
    void begin(Stream& input, Stream* output);

    void end();

    /* ── main loop — call every iteration ────────────────────────────── */
    void update();

    /* ── read received data ──────────────────────────────────────────── */
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

    /* ── callbacks (fire AFTER all bytes read — SoftwareSerial safe) ─── */
    void onReceive(RoboLinkValueCb cb);
    void onMessage(RoboLinkMsgCb cb);

    /* ── timeout ─────────────────────────────────────────────────────── */
    void setTimeoutMs(unsigned long ms);
    bool isTimedOut() const;
    bool hasReceivedData() const;

    /* ── status ──────────────────────────────────────────────────────── */
    bool          isStarted() const;
    unsigned long bytesReceived() const;
    unsigned long messagesReceived() const;

    /* ── advanced ────────────────────────────────────────────────────── */
    RoboLinkParser& parser();
    Stream*         stream();

private:
    void _doSend();         /* platform-aware send */
    void _drainRx();        /* read remaining bytes after TX */

    RoboLinkParser  _rx;
    RoboLinkParser  _sensors;
    Stream*         _input;
    Stream*         _output;
    bool            _started;

    RoboLinkValueCb _valueCb;
    RoboLinkMsgCb   _msgCb;

    unsigned long   _sendIntervalMs;
    unsigned long   _lastSendMs;
    unsigned long   _lastRxMs;       /* millis of last byte read */
    unsigned long   _bytesRx;
    unsigned long   _msgsRx;
};

#endif
