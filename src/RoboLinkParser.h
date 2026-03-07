/*
 * RoboLinkParser.h  —  v2.0
 * ──────────────────────────
 * Lean, zero-heap (AVR) key-value parser for the RoboLink protocol.
 *
 * Wire format:  key1:val1,key2:val2,...,keyN:valN\n
 *
 * Key design rule:
 *   feed() does ONLY buffering + parsing.  No callbacks.  No heap.
 *   Returns true when a complete message is parsed, so the caller
 *   (transport layer) can fire callbacks AFTER fully draining the
 *   serial buffer — preventing SoftwareSerial byte loss.
 */

#ifndef ROBOLINK_PARSER_H
#define ROBOLINK_PARSER_H

#include <Arduino.h>

/* ── Tuneable limits ────────────────────────────────────────────────── */
#if defined(__AVR__)
  #ifndef ROBOLINK_MAX_PAIRS
  #define ROBOLINK_MAX_PAIRS      8
  #endif
  #ifndef ROBOLINK_MAX_KEY_LEN
  #define ROBOLINK_MAX_KEY_LEN   12
  #endif
  #ifndef ROBOLINK_LINE_BUF
  #define ROBOLINK_LINE_BUF     128
  #endif
#else
  #ifndef ROBOLINK_MAX_PAIRS
  #define ROBOLINK_MAX_PAIRS     64
  #endif
  #ifndef ROBOLINK_MAX_KEY_LEN
  #define ROBOLINK_MAX_KEY_LEN   24
  #endif
  #ifndef ROBOLINK_LINE_BUF
  #define ROBOLINK_LINE_BUF     512
  #endif
#endif

/* ── Callback types (used by transports, NOT by this parser) ────────── */
typedef void (*RoboLinkValueCb)(const char* key, int value);
typedef void (*RoboLinkMsgCb)();

/* ── Parser class ───────────────────────────────────────────────────── */
class RoboLinkParser {
public:
    RoboLinkParser();

    /*  Feed one byte.  Returns TRUE when a complete line was parsed.
     *  No callbacks — safe inside tight SoftwareSerial read loops. */
    bool feed(uint8_t c);

    /*  Feed a block. Returns number of complete messages parsed. */
    int  feed(const uint8_t* data, size_t len);

    /* ── read parsed data ────────────────────────────────────────────── */
    int         get(const char* key, int defaultVal = 0) const;
    bool        has(const char* key) const;
    int         count() const;
    const char* keyAt(int index) const;
    int         valueAt(int index) const;

    /* ── write (for outgoing sensor map) ─────────────────────────────── */
    void set(const char* key, int value);
    bool remove(const char* key);

    /* ── iterate / print ─────────────────────────────────────────────── */
    void forEach(RoboLinkValueCb cb) const;
    void printAll(Stream& out = Serial) const;

    /* ── build outgoing message into caller-supplied buffer ──────────── */
    size_t buildMessage(char* buf, size_t bufSize) const;
#if !defined(__AVR__)
    String buildMessage() const;          /* ESP32/ARM only — uses heap */
#endif

    /* ── timeout ─────────────────────────────────────────────────────── */
    void          setTimeoutMs(unsigned long ms);
    bool          isTimedOut() const;
    bool          hasReceivedData() const;
    unsigned long lastRxTime() const;

    /* ── change detection ────────────────────────────────────────────── */
    bool     changed(int index) const;     /* did key at index change? */
    uint64_t changedMask() const;           /* bitmask of changed keys (up to 64) */
    int      changedCount() const;          /* number of changed keys  */

    /* ── reset ───────────────────────────────────────────────────────── */
    void clear();

private:
    struct Pair {
        char key[ROBOLINK_MAX_KEY_LEN + 1];
        int  value;
    };
    Pair _pairs[ROBOLINK_MAX_PAIRS];
    int  _count;

    char _lineBuf[ROBOLINK_LINE_BUF];
    int  _lineIdx;
    bool _overflow;                /* line too long → skip until \n */

    uint64_t      _changedMask;    /* bitmask: bit i = key i changed (up to 64 keys) */
    unsigned long _lastRxMs;
    unsigned long _timeoutMs;

    void _parseLine();
    int  _find(const char* key) const;
};

#endif
