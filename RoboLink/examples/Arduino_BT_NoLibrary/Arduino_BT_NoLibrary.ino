/*
 * RoboLink — Arduino Bluetooth  (No Library)
 * ============================================
 * Protocol:  key1:val1,key2:val2,...\n   (plain text, one line per message)
 *
 *   int  v = rlRead("key")       <- get value from app
 *   rlWrite("key", value)        <- send value to app
 *   bool ok = connected()        <- is app live?
 *
 * Note: named rlRead / rlWrite (not read / write) to avoid
 *       conflicts with Arduino / POSIX built-in function names.
 *
 * Hardware:
 *   HC-05 TX -> Arduino pin 10  (SoftwareSerial RX)
 *   HC-05 RX -> Arduino pin 11  (SoftwareSerial TX)  <- voltage divider!
 */

#define _SS_MAX_RX_BUFF 128
#include <SoftwareSerial.h>

/* -- Config --------------------------------------------------------- */
const int  BT_RX           = 10;
const int  BT_TX           = 11;
const long BT_BAUD         = 9600;
const unsigned long TIMEOUT_MS = 1000;

/* -- Key-value store (internal) ------------------------------------- */
#define MAX_KEYS  8
#define KEY_LEN  12
struct KV { char k[KEY_LEN+1]; int v; };
static KV            store[MAX_KEYS];
static int           storeN = 0;
static unsigned long lastRx = 0;

SoftwareSerial bt(BT_RX, BT_TX);

/* ================================================================
 *  PUBLIC API
 * ================================================================ */

/* Read the latest value for a key  (returns 0 if key not seen yet) */
int rlRead(const char* key, int def = 0) {
    for (int i = 0; i < storeN; i++)
        if (!strcmp(store[i].k, key)) return store[i].v;
    return def;
}

/* Send a key-value to the app
 * Half-duplex guard: skips if RX was active < 15 ms ago           */
void rlWrite(const char* key, int value) {
    if (lastRx > 0 && millis() - lastRx < 15) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%s:%d\n", key, value);
    bt.print(buf);
}

/* True if a message arrived within TIMEOUT_MS                     */
bool connected() { return lastRx > 0 && (millis() - lastRx <= TIMEOUT_MS); }

/* ================================================================
 *  INTERNAL -- DO NOT EDIT BELOW
 * ================================================================ */
static void _upsert(const char* key, int val) {
    for (int i = 0; i < storeN; i++)
        if (!strcmp(store[i].k, key)) { store[i].v = val; return; }
    if (storeN < MAX_KEYS) {
        strncpy(store[storeN].k, key, KEY_LEN); store[storeN].k[KEY_LEN] = '\0';
        store[storeN].v = val; storeN++;
    }
}
static void _parse(char* line) {
    char* p = line;
    while (*p) {
        char* colon = strchr(p, ':'); if (!colon) break;
        int klen = colon - p;
        if (klen > 0 && klen <= KEY_LEN) {
            char key[KEY_LEN+1]; memcpy(key, p, klen); key[klen] = '\0';
            _upsert(key, (int)atol(colon + 1));
        }
        char* comma = strchr(colon + 1, ','); if (!comma) break; p = comma + 1;
    }
    lastRx = millis();
}
static char _buf[128]; static int _idx = 0; static bool _ovf = false;
static void _readBT() {
    while (bt.available()) {
        char c = bt.read();
        if (c == '\n' || c == '\r') {
            if (!_ovf && _idx > 0) { _buf[_idx] = '\0'; _parse(_buf); }
            _idx = 0; _ovf = false;
        } else if (!_ovf) {
            if (_idx < 127) _buf[_idx++] = c; else { _ovf = true; _idx = 0; }
        }
    }
}

/* ================================================================
 *  SETUP
 * ================================================================ */
void setup() {
    Serial.begin(115200);
    bt.begin(BT_BAUD);
    Serial.println(F("RoboLink BT ready -- waiting for connection..."));
}

/* ================================================================
 *  LOOP
 * ================================================================ */
void loop() {

    _readBT();   /* <-- MUST stay as the first line of loop() */

    /* ************************************************************
     * ***           >>> YOUR CODE STARTS HERE <<<             ***
     * ************************************************************
     *
     *  Read any key the app sends:
     *    int throttle = rlRead("throttle");  // -100 to +100
     *    int steer    = rlRead("steer");     // -100 to +100
     *    int fire     = rlRead("fire");      //  0 or 1 (button)
     *
     *  Send values back to the app:
     *    rlWrite("rpm",     1200);
     *    rlWrite("battery",   87);
     *
     *  Safety check  (always guard your actuators):
     *    if (!connected()) {
     *        // motors off, servos to default, etc.
     *    }
     * ************************************************************ */

    // --- Example (uncomment to try) ---
    // int throttle = rlRead("throttle");
    // int steer    = rlRead("steer");
    // int fire     = rlRead("fire");
    //
    // if (!connected()) { /* safe stop */ }
    //
    // rlWrite("rpm",     1200);
    // rlWrite("battery",   87);

    /* ************************************************************
     * ***            >>> YOUR CODE ENDS HERE <<<              ***
     * ************************************************************ */

    /* -- Status print every 2 s (no delay) ---------------------- */
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        if      (!lastRx)      Serial.println(F("[WAIT] No data yet."));
        else if (!connected()) Serial.println(F("[LOST] Timed out."));
        else {
            Serial.println(F("--- received data ---"));
            for (int i = 0; i < storeN; i++) {
                Serial.print(F("  ")); Serial.print(store[i].k);
                Serial.print(F(" = ")); Serial.println(store[i].v);
            }
        }
    }
}