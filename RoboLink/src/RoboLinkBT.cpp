/*
 * RoboLinkBT.cpp  —  v2.0
 * ────────────────────────
 * ESP32 Bluetooth Classic transport.
 * Same pattern: read all bytes first, callbacks after.
 */

#if defined(ESP32)

#include "RoboLinkBT.h"

RoboLinkBT::RoboLinkBT()
    : _started(false), _valueCb(nullptr), _msgCb(nullptr),
      _sendIntervalMs(0), _lastSendMs(0)
{}

bool RoboLinkBT::begin(const char* deviceName) {
    _started = _bt.begin(deviceName);
    return _started;
}

void RoboLinkBT::end() { _bt.end(); _started = false; }

/* ════════════════════════════════════════════════════════════════════ */
void RoboLinkBT::update() {
    if (!_started) return;

    /* 1. Drain all available BT bytes — tight loop */
    bool gotMsg = false;
    while (_bt.available()) {
        uint8_t c = (uint8_t)_bt.read();
        if (_rx.feed(c)) gotMsg = true;
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

    /* 3. Auto-send sensors */
    if (_sendIntervalMs > 0 && _sensors.count() > 0) {
        if (millis() - _lastSendMs >= _sendIntervalMs) {
            _lastSendMs = millis();
            sendSensors();
        }
    }
}

/* ── delegates ───────────────────────────────────────────────────────── */

int         RoboLinkBT::get(const char* k, int d) const { return _rx.get(k, d); }
bool        RoboLinkBT::has(const char* k) const        { return _rx.has(k); }
int         RoboLinkBT::dataCount() const               { return _rx.count(); }
const char* RoboLinkBT::keyAt(int i) const              { return _rx.keyAt(i); }
int         RoboLinkBT::valueAt(int i) const            { return _rx.valueAt(i); }
void        RoboLinkBT::forEach(RoboLinkValueCb cb) const { _rx.forEach(cb); }
void        RoboLinkBT::printAll(Stream& out) const     { _rx.printAll(out); }

void RoboLinkBT::setSensor(const char* k, int v) { _sensors.set(k, v); }
bool RoboLinkBT::removeSensor(const char* k)     { return _sensors.remove(k); }
void RoboLinkBT::setSendInterval(unsigned long ms){ _sendIntervalMs = ms; }
int  RoboLinkBT::sensorCount() const              { return _sensors.count(); }

void RoboLinkBT::sendSensors() {
    if (!_started || !_bt.hasClient() || _sensors.count() == 0) return;
    String msg = _sensors.buildMessage();
    _bt.print(msg);
}

void RoboLinkBT::printSensors(Stream& out) const { _sensors.printAll(out); }

void RoboLinkBT::onReceive(RoboLinkValueCb cb) { _valueCb = cb; }
void RoboLinkBT::onMessage(RoboLinkMsgCb cb)   { _msgCb   = cb; }

void RoboLinkBT::setTimeoutMs(unsigned long ms) { _rx.setTimeoutMs(ms); }
bool RoboLinkBT::isTimedOut() const              { return _rx.isTimedOut(); }
bool RoboLinkBT::hasReceivedData() const         { return _rx.hasReceivedData(); }

bool RoboLinkBT::isStarted() const { return _started; }
bool RoboLinkBT::hasClient() const { return _started && const_cast<BluetoothSerial&>(_bt).hasClient(); }

RoboLinkParser&  RoboLinkBT::parser() { return _rx; }
BluetoothSerial& RoboLinkBT::serial() { return _bt; }

#endif /* ESP32 */
