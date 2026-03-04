/*
 * RoboLink.h  —  v2.0
 * ────────────────────
 * Main include header.
 *
 * Includes the core parser + the platform-appropriate transport(s).
 *
 *   Arduino / AVR  →  RoboLinkSerial  (SoftwareSerial / HardwareSerial)
 *   ESP32          →  RoboLinkSerial + RoboLinkBT + RoboLinkWiFi
 */

#ifndef ROBOLINK_H
#define ROBOLINK_H

#define ROBOLINK_VERSION "2.0.0"

/* ── Always available ────────────────────────────────────────────────── */
#include "RoboLinkParser.h"
#include "RoboLinkSerial.h"

/* ── ESP32-only transports ───────────────────────────────────────────── */
#if defined(ESP32)
  #include "RoboLinkBT.h"
  #include "RoboLinkWiFi.h"
#endif

#endif
