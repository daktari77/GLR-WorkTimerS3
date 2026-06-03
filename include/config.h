#pragma once

// ===================== Firmware =====================
#define FW_VERSION       "1.3.0"

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
#define PIN_BAT_ADC      4    // partitore batteria T-Display S3 (Vbatt = ADC*2)

// LiPo: tensione (mV, al pacco) per 0% e 100% carica
#define BAT_MV_EMPTY     3300
#define BAT_MV_FULL      4200

// ===================== Schermo / comportamento =====================
#define SCREEN_BRIGHT    200  // luminosita' backlight di default (0-255)
#define POMO_AUTO_ADV    1    // 1 = pomodoro avanza da solo tra le fasi

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
