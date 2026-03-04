/*
 * RoboLinkParser.cpp  —  v2.0
 * ────────────────────────────
 * Lean line parser.  feed() does zero allocations and calls no
 * callbacks — it just buffers bytes and parses on newline.
 */

#include "RoboLinkParser.h"
#include <string.h>

/* ════════════════════════════════════════════════════════════════════ */
RoboLinkParser::RoboLinkParser()
    : _count(0), _lineIdx(0), _overflow(false),
      _changedMask(0), _lastRxMs(0), _timeoutMs(0)
{
    memset(_pairs, 0, sizeof(_pairs));
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Feed                                                                */
/* ════════════════════════════════════════════════════════════════════ */

bool RoboLinkParser::feed(uint8_t c) {
    if (c == '\n' || c == '\r') {
        if (_overflow) {
            /* Line was too long — discard it, start fresh */
            _overflow = false;
            _lineIdx  = 0;
            return false;
        }
        if (_lineIdx > 0) {
            _lineBuf[_lineIdx] = '\0';
            _parseLine();
            _lineIdx  = 0;
            _lastRxMs = millis();
            return true;               /* ← complete message */
        }
        return false;
    }

    if (_overflow) return false;        /* skip until newline */

    if (_lineIdx < ROBOLINK_LINE_BUF - 1) {
        _lineBuf[_lineIdx++] = (char)c;
    } else {
        /* Buffer full — mark overflow, we'll discard at next \n */
        _overflow = true;
        _lineIdx  = 0;
    }
    return false;
}

int RoboLinkParser::feed(const uint8_t* data, size_t len) {
    int msgs = 0;
    for (size_t i = 0; i < len; i++) {
        if (feed(data[i])) msgs++;
    }
    return msgs;
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Line parsing                                                        */
/* ════════════════════════════════════════════════════════════════════ */

void RoboLinkParser::_parseLine() {
    _changedMask = 0ULL;           /* reset change tracking */
    char* ptr = _lineBuf;
    while (*ptr) {
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == '\0') break;

        char* colon = strchr(ptr, ':');
        if (!colon) break;

        int keyLen = (int)(colon - ptr);
        if (keyLen <= 0 || keyLen > ROBOLINK_MAX_KEY_LEN) {
            char* comma = strchr(colon, ',');
            if (comma) { ptr = comma + 1; continue; }
            else break;
        }

        long value = atol(colon + 1);

        char keyBuf[ROBOLINK_MAX_KEY_LEN + 1];
        memcpy(keyBuf, ptr, (size_t)keyLen);
        keyBuf[keyLen] = '\0';

        int idx = _find(keyBuf);
        if (idx >= 0) {
            if (_pairs[idx].value != (int)value) {
                _pairs[idx].value = (int)value;
                _changedMask |= ((uint64_t)1 << idx);
            }
        } else if (_count < ROBOLINK_MAX_PAIRS) {
            strncpy(_pairs[_count].key, keyBuf, ROBOLINK_MAX_KEY_LEN);
            _pairs[_count].key[ROBOLINK_MAX_KEY_LEN] = '\0';
            _pairs[_count].value = (int)value;
            _changedMask |= ((uint64_t)1 << _count);
            _count++;
        }

        char* comma = strchr(colon + 1, ',');
        if (comma) ptr = comma + 1;
        else break;
    }
}

int RoboLinkParser::_find(const char* key) const {
    for (int i = 0; i < _count; i++) {
        if (strcmp(_pairs[i].key, key) == 0) return i;
    }
    return -1;
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Data access                                                         */
/* ════════════════════════════════════════════════════════════════════ */

int  RoboLinkParser::get(const char* key, int d) const {
    int i = _find(key);  return i >= 0 ? _pairs[i].value : d;
}
bool        RoboLinkParser::has(const char* key) const { return _find(key) >= 0; }
int         RoboLinkParser::count() const              { return _count; }
const char* RoboLinkParser::keyAt(int i) const         { return (i >= 0 && i < _count) ? _pairs[i].key : ""; }
int         RoboLinkParser::valueAt(int i) const       { return (i >= 0 && i < _count) ? _pairs[i].value : 0; }

/* ════════════════════════════════════════════════════════════════════ */
/*  Manual set / remove                                                 */
/* ════════════════════════════════════════════════════════════════════ */

void RoboLinkParser::set(const char* key, int value) {
    int idx = _find(key);
    if (idx >= 0) {
        _pairs[idx].value = value;
    } else if (_count < ROBOLINK_MAX_PAIRS) {
        strncpy(_pairs[_count].key, key, ROBOLINK_MAX_KEY_LEN);
        _pairs[_count].key[ROBOLINK_MAX_KEY_LEN] = '\0';
        _pairs[_count].value = value;
        _count++;
    }
}

bool RoboLinkParser::remove(const char* key) {
    int idx = _find(key);
    if (idx < 0) return false;
    if (idx < _count - 1) _pairs[idx] = _pairs[_count - 1];
    _count--;
    return true;
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Iterate / print                                                     */
/* ════════════════════════════════════════════════════════════════════ */

void RoboLinkParser::forEach(RoboLinkValueCb cb) const {
    if (!cb) return;
    for (int i = 0; i < _count; i++) cb(_pairs[i].key, _pairs[i].value);
}

void RoboLinkParser::printAll(Stream& out) const {
    for (int i = 0; i < _count; i++) {
        out.print(F("  "));
        out.print(_pairs[i].key);
        out.print(F(" = "));
        out.println(_pairs[i].value);
    }
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Build outgoing message                                              */
/* ════════════════════════════════════════════════════════════════════ */

size_t RoboLinkParser::buildMessage(char* buf, size_t bufSize) const {
    if (_count == 0 || bufSize < 2) { if (bufSize) buf[0] = '\0'; return 0; }
    size_t pos = 0;
    for (int i = 0; i < _count; i++) {
        int w = snprintf(buf + pos, bufSize - pos,
                         (i < _count - 1) ? "%s:%d," : "%s:%d",
                         _pairs[i].key, _pairs[i].value);
        if (w < 0 || (size_t)w >= bufSize - pos - 1) break;
        pos += (size_t)w;
    }
    if (pos + 1 < bufSize) { buf[pos++] = '\n'; }
    buf[pos] = '\0';
    return pos;
}

#if !defined(__AVR__)
String RoboLinkParser::buildMessage() const {
    String msg;
    msg.reserve((size_t)(_count * 16));
    for (int i = 0; i < _count; i++) {
        if (i > 0) msg += ',';
        msg += _pairs[i].key;
        msg += ':';
        msg += String(_pairs[i].value);
    }
    msg += '\n';
    return msg;
}
#endif

/* ════════════════════════════════════════════════════════════════════ */
/*  Timeout                                                             */
/* ════════════════════════════════════════════════════════════════════ */

void          RoboLinkParser::setTimeoutMs(unsigned long ms) { _timeoutMs = ms; }
bool          RoboLinkParser::isTimedOut() const { return _timeoutMs && _lastRxMs && (millis() - _lastRxMs > _timeoutMs); }
bool          RoboLinkParser::hasReceivedData() const { return _lastRxMs > 0; }
unsigned long RoboLinkParser::lastRxTime() const { return _lastRxMs; }

/* ════════════════════════════════════════════════════════════════════ */
/*  Change detection                                                    */
/* ════════════════════════════════════════════════════════════════════ */

bool     RoboLinkParser::changed(int i) const  { return (i >= 0 && i < _count) && (_changedMask & ((uint64_t)1 << i)); }
uint64_t RoboLinkParser::changedMask() const    { return _changedMask; }
int      RoboLinkParser::changedCount() const   {
    uint32_t m = _changedMask; int n = 0;
    while (m) { n += (m & 1); m >>= 1; }
    return n;
}

void RoboLinkParser::clear() {
    _count = 0; _lineIdx = 0; _overflow = false; _changedMask = 0;
    memset(_pairs, 0, sizeof(_pairs));
}
