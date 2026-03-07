/*
 * RoboLink — ESP32 WiFi  (No Library)
 * ======================================
 * Protocol:  key1:val1,key2:val2,...\n   (plain text over UDP)
 *
 *   int  v = rlRead("key")       <- get value from app
 *   rlWrite("key", value)        <- send value to app
 *   bool ok = connected()        <- is app live?
 *
 * Note: named rlRead / rlWrite (not read / write) to avoid
 *       conflicts with POSIX built-in function names on ESP32.
 *
 * Hardware: ESP32 (any variant)
 *
 * App setup:
 *   1. Connect phone to the WiFi AP created by the ESP32.
 *   2. Open RoboLink app -> WiFi -> connect.
 *   3. Add widgets; key names appear in Serial Monitor.
 */

#include <WiFi.h>
#include <WiFiUdp.h>

/* -- Config --------------------------------------------------------- */
const char* WIFI_SSID      = "RoboLink";
const char* WIFI_PASSWORD  = "12345678";
const int   UDP_PORT       = 4210;
const unsigned long TIMEOUT_MS = 500;

/* -- Key-value store (internal) ------------------------------------- */
#define MAX_KEYS  100
#define KEY_LEN   24
struct KV { char k[KEY_LEN+1]; int v; };
static KV            store[MAX_KEYS];
static int           storeN     = 0;
static unsigned long lastRx     = 0;
static IPAddress     remoteIP;
static uint16_t      remotePort = 0;
static bool          hasRemote  = false;
WiFiUDP udp;

/* ================================================================
 *  PUBLIC API
 * ================================================================ */

/* Read the latest value for a key  (returns 0 if key not seen yet) */
int rlRead(const char* key, int def = 0) {
    for (int i = 0; i < storeN; i++)
        if (!strcmp(store[i].k, key)) return store[i].v;
    return def;
}

/* Send a key-value to the app via UDP reply                        */
void rlWrite(const char* key, int value) {
    if (!hasRemote) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "%s:%d\n", key, value);
    udp.beginPacket(remoteIP, remotePort);
    udp.write((const uint8_t*)buf, strlen(buf));
    udp.endPacket();
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
static uint8_t _pkt[513];
static void _readUDP() {
    int sz;
    while ((sz = udp.parsePacket()) > 0) {
        int len = udp.read(_pkt, sizeof(_pkt) - 1);
        if (len <= 0) continue;
        _pkt[len] = '\0';
        remoteIP   = udp.remoteIP();
        remotePort = udp.remotePort();
        hasRemote  = true;
        /* split into lines and parse each one */
        char* start = (char*)_pkt;
        char* nl;
        while ((nl = strchr(start, '\n')) != nullptr) {
            *nl = '\0'; if (nl > start) _parse(start); start = nl + 1;
        }
        if (*start) _parse(start);
    }
}

/* ================================================================
 *  SETUP
 * ================================================================ */
void setup() {
    Serial.begin(115200);
    delay(500);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    delay(100);
    udp.begin(UDP_PORT);
    Serial.print(F("WiFi AP: "));  Serial.println(WIFI_SSID);
    Serial.print(F("IP:      "));  Serial.println(WiFi.softAPIP());
    Serial.print(F("Port:    "));  Serial.println(UDP_PORT);
    Serial.println(F("Connect phone to this WiFi, open RoboLink app."));
}

/* ================================================================
 *  LOOP
 * ================================================================ */
void loop() {

    _readUDP();   /* <-- MUST stay as the first line of loop() */

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
        int clients = (int)WiFi.softAPgetStationNum();
        if      (clients == 0) Serial.println(F("[WAIT] No phone on WiFi."));
        else if (!lastRx)      Serial.println(F("[WAIT] Waiting for app data."));
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