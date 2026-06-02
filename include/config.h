#pragma once

// ===================== WiFi / NTP =====================
// Config WiFi via portale captive (WiFiManager): niente credenziali hardcoded.
// Primo avvio (o nessuna rete salvata) crea AP. Connettiti e inserisci SSID/pass.
// Forza il portale tenendo premuto BOOT durante l'accensione.
#define AP_NAME          "WorkTimer-Setup"
#define AP_PASS          "worktimer"   // min 8 caratteri, "" per AP aperto
#define PORTAL_TIMEOUT_S 120           // chiude portale dopo N secondi
#define NTP_SERVER       "pool.ntp.org"
// Offset fuso orario in secondi. Italia: CET = 3600, CEST (estate) = 7200
#define GMT_OFFSET_SEC   3600
#define DST_OFFSET_SEC   3600   // ora legale: +3600. Inverno: 0

// ===================== Stato persistente =====================
#define STATE_SAVE_MS    10000  // salvataggio periodico stato mentre attivo

// ===================== Hardware =====================
// T-Display S3: il pin PIN_POWER_ON deve essere HIGH per alimentare LCD.
#define PIN_POWER_ON     15
#define PIN_BTN_KEY      14   // bottone in basso (azione primaria)
#define PIN_BTN_BOOT     0    // bottone in alto (cambio modo / stop)

// ===================== Bottoni =====================
#define DEBOUNCE_MS      40
#define LONGPRESS_MS     700

// ===================== Pomodoro =====================
#define POMO_WORK_MIN    25
#define POMO_BREAK_MIN   5
#define POMO_LONGBREAK_MIN 15
#define POMO_CYCLES_TO_LONG 4   // dopo N work, pausa lunga

// ===================== Log =====================
#define LOG_PATH         "/sessions.csv"
