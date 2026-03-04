/*
 * RoboLinkSerial.cpp  —  v2.0
 * ────────────────────────────
 * The critical fix:  update() reads ALL available bytes into the
 * parser FIRST (tight loop, no interruptions), THEN fires any
 * user callbacks.  This ensures SoftwareSerial's tiny RX buffer
 * is drained before anything else blocks the CPU.
 */

#include "RoboLinkSerial.h"

/* ════════════════════════════════════════════════════════════════════ */

RoboLinkSerial::RoboLinkSerial()
    : _input(nullptr), _output(nullptr), _started(false),
      _valueCb(nullptr), _msgCb(nullptr),
      _sendIntervalMs(0), _lastSendMs(0), _lastRxMs(0),
      _bytesRx(0), _msgsRx(0)
{}

/* ── init ────────────────────────────────────────────────────────────── */

void RoboLinkSerial::begin(Stream& stream) {
    _input = &stream; _output = &stream;
    _started = true; _lastSendMs = millis();
    _bytesRx = 0; _msgsRx = 0; _lastRxMs = 0;
}

void RoboLinkSerial::begin(Stream& input, Stream* output) {
    _input = &input; _output = output;
    _started = true; _lastSendMs = millis();
    _bytesRx = 0; _msgsRx = 0; _lastRxMs = 0;
}

void RoboLinkSerial::end() {
    _input = nullptr; _output = nullptr; _started = false;
}

/* ════════════════════════════════════════════════════════════════════ */
/*  update()  — THE HOT PATH                                           */
/* ════════════════════════════════════════════════════════════════════ */

void RoboLinkSerial::update() {
    if (!_started || !_input) return;

    /*
     * STEP 1 — Drain ALL available bytes.
     * This is a tight loop with ZERO side-effects (no callbacks,
     * no Serial.print, no heap alloc).  On SoftwareSerial, every
     * microsecond counts — the 64-byte RX buffer fills in ~67 ms
     * at 9600 baud.
     */
    bool gotMsg  = false;
    bool gotByte = false;

    while (_input->available()) {
        uint8_t c = (uint8_t)_input->read();
        _bytesRx++;
        gotByte = true;
        if (_rx.feed(c)) {
            gotMsg = true;
            _msgsRx++;
        }
    }

    unsigned long now = millis();
    if (gotByte) _lastRxMs = now;

    /*
     * STEP 2 — Fire callbacks AFTER the buffer is empty.
     * Even if these do Serial.print() and block for milliseconds,
     * no bytes are lost because we already drained everything.
     */
    if (gotMsg) {
        if (_valueCb) {
            /* Only fire for keys that actually changed — reduces
               Serial.print volume and prevents SoftwareSerial
               RX overflow on AVR.  */
            uint32_t mask = _rx.changedMask();
            for (int i = 0; i < _rx.count() && mask; i++) {
                if (mask & ((uint32_t)1 << i)) {
                    _valueCb(_rx.keyAt(i), _rx.valueAt(i));
                    mask &= ~((uint32_t)1 << i);
                }
            }
        }
        if (_msgCb) _msgCb();
    }

    /*
     * STEP 3 — Auto-send sensor data (if configured).
     * On AVR with SoftwareSerial, TX blocks RX interrupts.
     * We only send when the line has been quiet for ≥ 15 ms
     * (no bytes arriving) to minimise the chance of losing data.
     */
    if (_sendIntervalMs > 0 && _output && _sensors.count() > 0) {
        if (now - _lastSendMs >= _sendIntervalMs) {
#if defined(__AVR__)
            /* Half-duplex guard: wait for quiet period */
            if (_lastRxMs == 0 || (now - _lastRxMs >= 15)) {
                _lastSendMs = now;
                _doSend();
            }
#else
            _lastSendMs = now;
            _doSend();
#endif
        }
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Send helpers                                                        */
/* ════════════════════════════════════════════════════════════════════ */

void RoboLinkSerial::_doSend() {
    if (!_output || _sensors.count() == 0) return;

    /* Build message into a stack buffer — no heap on AVR */
    char buf[96];
    size_t len = _sensors.buildMessage(buf, sizeof(buf));
    if (len > 0) {
        _output->write((const uint8_t*)buf, len);
    }

#if defined(__AVR__)
    /* SoftwareSerial TX just blocked interrupts — drain any bytes
       that accumulated in the hardware shift register. */
    _drainRx();
#endif
}

void RoboLinkSerial::_drainRx() {
    if (!_input) return;
    while (_input->available()) {
        _bytesRx++;
        _rx.feed((uint8_t)_input->read());
    }
}

void RoboLinkSerial::sendSensors() { _doSend(); }

/* ════════════════════════════════════════════════════════════════════ */
/*  Delegates                                                           */
/* ════════════════════════════════════════════════════════════════════ */

int         RoboLinkSerial::get(const char* k, int d) const { return _rx.get(k, d); }
bool        RoboLinkSerial::has(const char* k) const        { return _rx.has(k); }
int         RoboLinkSerial::dataCount() const               { return _rx.count(); }
const char* RoboLinkSerial::keyAt(int i) const              { return _rx.keyAt(i); }
int         RoboLinkSerial::valueAt(int i) const            { return _rx.valueAt(i); }
void        RoboLinkSerial::forEach(RoboLinkValueCb cb) const { _rx.forEach(cb); }
void        RoboLinkSerial::printAll(Stream& out) const     { _rx.printAll(out); }

void RoboLinkSerial::setSensor(const char* k, int v) { _sensors.set(k, v); }
bool RoboLinkSerial::removeSensor(const char* k)     { return _sensors.remove(k); }
void RoboLinkSerial::setSendInterval(unsigned long ms){ _sendIntervalMs = ms; }
int  RoboLinkSerial::sensorCount() const              { return _sensors.count(); }

void RoboLinkSerial::printSensors(Stream& out) const { _sensors.printAll(out); }

void RoboLinkSerial::onReceive(RoboLinkValueCb cb) { _valueCb = cb; }
void RoboLinkSerial::onMessage(RoboLinkMsgCb cb)   { _msgCb   = cb; }

void RoboLinkSerial::setTimeoutMs(unsigned long ms) { _rx.setTimeoutMs(ms); }
bool RoboLinkSerial::isTimedOut() const              { return _rx.isTimedOut(); }
bool RoboLinkSerial::hasReceivedData() const         { return _rx.hasReceivedData(); }

bool          RoboLinkSerial::isStarted() const       { return _started; }
unsigned long RoboLinkSerial::bytesReceived() const    { return _bytesRx; }
unsigned long RoboLinkSerial::messagesReceived() const { return _msgsRx; }

RoboLinkParser& RoboLinkSerial::parser() { return _rx; }
Stream*         RoboLinkSerial::stream() { return _input; }
