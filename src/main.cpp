// ============================================================
//  GLR-WorkTimer  -  LilyGo T-Display S3 (ESP32-S3)
//  Modi: CLOCK (idle) / STOPWATCH / POMODORO
//  KEY  (GPIO14): start / pausa / riprendi   | longpress: stop+salva
//  BOOT (GPIO0) : cambia modo (solo se fermo) | longpress in clock: reset log
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Preferences.h>
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <vector>
#include "config.h"

// ---------------- Display ----------------
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);   // back-buffer full screen
static const int SCR_W = 320;
static const int SCR_H = 170;

// ---------------- Stato app ----------------
enum Mode  { MODE_CLOCK, MODE_STOPWATCH, MODE_POMODORO, MODE_COUNT };
enum RunSt { ST_IDLE, ST_RUNNING, ST_PAUSED };
enum PomoPhase { PH_WORK, PH_BREAK, PH_LONGBREAK };

Mode  mode   = MODE_CLOCK;
RunSt runSt  = ST_IDLE;

// tempo accumulato (ms) + riferimento avvio segmento corrente
uint32_t accumMs   = 0;     // ms gia' accumulati nei segmenti chiusi
uint32_t segStart  = 0;     // millis() all'avvio segmento attivo

// pomodoro
PomoPhase pomoPhase   = PH_WORK;
int       pomoDoneWork = 0;       // work completati nel ciclo
uint32_t  pomoTargetMs = 0;       // durata fase corrente

bool timeSynced = false;
bool fsReady    = false;

Preferences prefs;
uint32_t lastStateSave = 0;

// config pomodoro (minuti) - modificabile via web, persistita in NVS
int cfgWork   = POMO_WORK_MIN;
int cfgBreak  = POMO_BREAK_MIN;
int cfgLong   = POMO_LONGBREAK_MIN;
int cfgCycles = POMO_CYCLES_TO_LONG;

// config estesa (web + NVS)
int  cfgGmtMin  = GMT_OFFSET_SEC / 60;   // offset fuso (minuti)
int  cfgDstMin  = DST_OFFSET_SEC / 60;   // offset ora legale (minuti)
bool cfgAutoAdv = POMO_AUTO_ADV;         // pomodoro avanza da solo tra le fasi
int  cfgBright  = SCREEN_BRIGHT;         // luminosita' backlight 0-255
bool cfgBuzz    = BUZZER_DEFAULT;        // buzzer fine pomodoro
static const uint8_t BL_CH  = 0;         // canale LEDC backlight
static const uint8_t BUZ_CH = 1;         // canale LEDC buzzer

// stato sleep schermo (idle -> backlight off, wake su tasto)
bool     screenAsleep   = false;
uint32_t lastActivityMs = 0;

void applyBright() { ledcWrite(BL_CH, cfgBright); }

// buzzer passivo: un tono per `ms` ms (no-op se disabilitato o pin assente)
void buzzTone(uint16_t freq, uint16_t ms) {
  if (!cfgBuzz || BUZZER_PIN < 0) return;
  ledcWriteTone(BUZ_CH, freq);
  delay(ms);
  ledcWriteTone(BUZ_CH, 0);
}
// melodia fine fase: salita se finita una fase di lavoro, discesa per le pause
void buzzPomodoro(bool workEnded) {
  if (workEnded) { buzzTone(880, 120); buzzTone(1320, 200); }
  else           { buzzTone(660, 120); buzzTone(440, 200); }
}

// risveglia lo schermo dallo stato attivo
void wakeScreen() {
  if (screenAsleep) { screenAsleep = false; applyBright(); }
  lastActivityMs = millis();
}

// lettura batteria: mV al pacco (partitore /2), media di 8 campioni
int batteryMvRaw() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BAT_ADC);
  return (int)((sum / 8) * 2);
}

// versione con debounce: campiona l'ADC al massimo una volta ogni 5s,
// restituisce il valore in cache fra una lettura e l'altra
int batteryMv() {
  static int cachedMv = 0;
  static uint32_t lastReadMs = 0;
  uint32_t now = millis();
  if (cachedMv == 0 || (uint32_t)(now - lastReadMs) >= 5000) {
    cachedMv = batteryMvRaw();
    lastReadMs = now;
  }
  return cachedMv;
}

// pacco LiPo presente? (sotto ~2.5V al pacco = USB senza batteria / lettura nulla)
bool batteryPresent(int mv) { return mv > 2500; }

// rete / web server
WebServer server(80);
bool   wifiUp = false;
String ipStr  = "";

// ---------------- Bottoni ----------------
struct Button {
  uint8_t pin;
  bool    lastRaw;
  bool    stable;
  uint32_t tChange;
  uint32_t tPress;
  bool    longFired;
};
Button bKey  { PIN_BTN_KEY,  true, true, 0, 0, false };
Button bBoot { PIN_BTN_BOOT, true, true, 0, 0, false };

// eventi prodotti dal poll
enum BtnEvent { EV_NONE, EV_SHORT, EV_LONG };

BtnEvent pollButton(Button &b) {
  bool raw = digitalRead(b.pin);          // active LOW
  uint32_t now = millis();
  BtnEvent ev = EV_NONE;
  if (raw != b.lastRaw) { b.lastRaw = raw; b.tChange = now; }
  if ((now - b.tChange) > DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    if (b.stable == LOW) {                 // premuto
      b.tPress = now; b.longFired = false;
    } else {                               // rilasciato
      if (!b.longFired) ev = EV_SHORT;
    }
  }
  // longpress mentre tenuto
  if (b.stable == LOW && !b.longFired && (now - b.tPress) > LONGPRESS_MS) {
    b.longFired = true; ev = EV_LONG;
  }
  return ev;
}

// ---------------- Helpers tempo ----------------
uint32_t elapsedMs() {
  uint32_t e = accumMs;
  if (runSt == ST_RUNNING) e += (millis() - segStart);
  return e;
}

void fmtHMS(uint32_t ms, char *out, size_t n) {
  uint32_t s = ms / 1000;
  uint32_t h = s / 3600; s %= 3600;
  uint32_t m = s / 60;   s %= 60;
  if (h > 0) snprintf(out, n, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  else       snprintf(out, n, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

const char* modeName(Mode m) {
  switch (m) { case MODE_CLOCK: return "OROLOGIO";
               case MODE_STOPWATCH: return "CRONOMETRO";
               case MODE_POMODORO: return "POMODORO";
               default: return "?"; }
}
const char* phaseName(PomoPhase p) {
  switch (p) { case PH_WORK: return "LAVORO";
               case PH_BREAK: return "PAUSA";
               case PH_LONGBREAK: return "PAUSA LUNGA";
               default: return "?"; }
}

// ---------------- Log sessioni ----------------
void logSession(const char* kind, uint32_t durMs) {
  if (!fsReady || durMs < 1000) return;
  File f = LittleFS.open(LOG_PATH, FILE_APPEND);
  if (!f) return;
  char ts[32] = "no-time";
  if (timeSynced) {
    time_t t = time(nullptr);
    struct tm tmv; localtime_r(&t, &tmv);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
  }
  uint32_t s = durMs / 1000;
  f.printf("%s,%s,%lu\n", ts, kind, (unsigned long)s);
  f.close();
  Serial.printf("[LOG] %s %s %lus\n", ts, kind, (unsigned long)s);
}

int countSessions() {
  if (!fsReady) return 0;
  File f = LittleFS.open(LOG_PATH, FILE_READ);
  if (!f) return 0;
  int n = 0;
  while (f.available()) { if (f.read() == '\n') n++; }
  f.close();
  return n;
}

// ---------------- Stato persistente (NVS) ----------------
// Salva snapshot. Timer salvato come elapsed + flag "active": al reboot
// (millis azzerato) si ripristina in PAUSA, l'utente riprende con KEY.
void saveState() {
  prefs.putUChar("mode",  (uint8_t)mode);
  prefs.putUChar("phase", (uint8_t)pomoPhase);
  prefs.putInt  ("done",  pomoDoneWork);
  prefs.putUInt ("elaps", elapsedMs());
  prefs.putBool ("active", runSt != ST_IDLE);
  lastStateSave = millis();
}

void loadState() {
  mode         = (Mode)prefs.getUChar("mode", MODE_CLOCK);
  if (mode >= MODE_COUNT) mode = MODE_CLOCK;
  PomoPhase ph = (PomoPhase)prefs.getUChar("phase", PH_WORK);
  pomoDoneWork = prefs.getInt("done", 0);
  uint32_t el  = prefs.getUInt("elaps", 0);
  bool active  = prefs.getBool("active", false);

  pomoPhase = ph;            // pomoSetPhase chiamato in setup dopo
  if (active && mode != MODE_CLOCK) {
    accumMs = el; runSt = ST_PAUSED;   // ripristino in pausa
  } else {
    accumMs = 0; runSt = ST_IDLE;
  }
}

// ---------------- Config pomodoro (NVS) ----------------
void loadConfig() {
  cfgWork   = prefs.getInt("cwork",  POMO_WORK_MIN);
  cfgBreak  = prefs.getInt("cbreak", POMO_BREAK_MIN);
  cfgLong   = prefs.getInt("clong",  POMO_LONGBREAK_MIN);
  cfgCycles = prefs.getInt("ccyc",   POMO_CYCLES_TO_LONG);
  cfgGmtMin  = prefs.getInt ("cgmt",    GMT_OFFSET_SEC / 60);
  cfgDstMin  = prefs.getInt ("cdst",    DST_OFFSET_SEC / 60);
  cfgAutoAdv = prefs.getBool("cauto",   POMO_AUTO_ADV);
  cfgBright  = prefs.getInt ("cbright", SCREEN_BRIGHT);
  cfgBuzz    = prefs.getBool("cbuzz",   BUZZER_DEFAULT);
}

void saveConfig() {
  prefs.putInt("cwork",  cfgWork);
  prefs.putInt("cbreak", cfgBreak);
  prefs.putInt("clong",  cfgLong);
  prefs.putInt("ccyc",   cfgCycles);
  prefs.putInt ("cgmt",    cfgGmtMin);
  prefs.putInt ("cdst",    cfgDstMin);
  prefs.putBool("cauto",   cfgAutoAdv);
  prefs.putInt ("cbright", cfgBright);
  prefs.putBool("cbuzz",   cfgBuzz);
}

// ---------------- Pomodoro logica ----------------
void pomoSetPhase(PomoPhase p) {
  pomoPhase = p;
  switch (p) {
    case PH_WORK:      pomoTargetMs = (uint32_t)cfgWork  * 60000UL; break;
    case PH_BREAK:     pomoTargetMs = (uint32_t)cfgBreak * 60000UL; break;
    case PH_LONGBREAK: pomoTargetMs = (uint32_t)cfgLong  * 60000UL; break;
  }
}

void resetSegment() { accumMs = 0; segStart = millis(); }

// transizione di fase: logga il work completato e imposta la fase successiva
void pomoNextPhase() {
  if (pomoPhase == PH_WORK) {
    logSession("pomodoro-work", pomoTargetMs);
    pomoDoneWork++;
    if (pomoDoneWork % cfgCycles == 0) pomoSetPhase(PH_LONGBREAK);
    else                               pomoSetPhase(PH_BREAK);
  } else {
    pomoSetPhase(PH_WORK);
  }
}

// fine countdown: avanza alla fase successiva.
// cfgAutoAdv=true -> riparte da solo; false -> resta in pausa, attende KEY.
void pomoAdvance() {
  pomoNextPhase();
  if (cfgAutoAdv) { resetSegment(); runSt = ST_RUNNING; }
  else            { accumMs = 0;    runSt = ST_PAUSED;  }
  saveState();
}

// ---------------- Azioni bottoni ----------------
void actionPrimary() {        // KEY short
  switch (runSt) {
    case ST_IDLE:
      if (mode == MODE_CLOCK) return;     // niente da avviare
      accumMs = 0; segStart = millis(); runSt = ST_RUNNING;
      if (mode == MODE_POMODORO) pomoSetPhase(pomoPhase); // assicura target
      break;
    case ST_RUNNING:
      accumMs += (millis() - segStart); runSt = ST_PAUSED;
      break;
    case ST_PAUSED:
      segStart = millis(); runSt = ST_RUNNING;
      break;
  }
  saveState();
}

void actionStop() {           // KEY long: ferma e salva
  if (runSt == ST_IDLE) return;
  uint32_t dur = elapsedMs();
  if (mode == MODE_STOPWATCH) logSession("stopwatch", dur);
  else if (mode == MODE_POMODORO) logSession("pomodoro-partial", dur);
  runSt = ST_IDLE; accumMs = 0;
  if (mode == MODE_POMODORO) { pomoDoneWork = 0; pomoSetPhase(PH_WORK); }
  saveState();
}

void actionModeSwitch() {     // BOOT short: cambia modo (solo se fermo)
  if (runSt != ST_IDLE) return;
  mode = (Mode)((mode + 1) % MODE_COUNT);
  if (mode == MODE_POMODORO) { pomoDoneWork = 0; pomoSetPhase(PH_WORK); }
  saveState();
}

void actionResetLog() {       // BOOT long in clock: cancella log
  if (mode == MODE_CLOCK && runSt == ST_IDLE && fsReady) {
    LittleFS.remove(LOG_PATH);
    Serial.println("[LOG] reset");
  }
}

// ---------------- UI ----------------
uint16_t phaseColor() {
  if (mode != MODE_POMODORO) return TFT_CYAN;
  switch (pomoPhase) {
    case PH_WORK:      return TFT_RED;
    case PH_BREAK:     return TFT_GREEN;
    case PH_LONGBREAK: return TFT_BLUE;
  }
  return TFT_WHITE;
}

// grigio discreto per etichette (leggibile ma defilato)
static const uint16_t DIM = 0x8410;   // ~ rgb(130,130,130)

// disegna il tempo grande centrato, scegliendo il font numerico piu' grande
// che entra in larghezza (font8 ~75px, font7 ~48px LCD).
void drawBigTime(const char* str, int cy, uint16_t col) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(col);
  spr.setTextSize((strlen(str) <= 5) ? 2 : 1);  // ingrandisce le cifre corte (HH:MM / MM:SS)
  spr.drawString(str, SCR_W/2, cy, 7);          // sempre Font7 (7-segmenti)
  spr.setTextSize(1);
}

void drawClock() {
  spr.setTextDatum(MC_DATUM);
  char buf[40];
  if (timeSynced) {
    time_t t = time(nullptr); struct tm tmv; localtime_r(&t, &tmv);
    // ora grande bianca
    strftime(buf, sizeof(buf), "%H:%M", &tmv);
    drawBigTime(buf, 78, TFT_WHITE);
    // data discreta
    strftime(buf, sizeof(buf), "%a %d %b %Y", &tmv);
    spr.setTextColor(DIM, TFT_BLACK);
    spr.drawString(buf, SCR_W/2, 150, 2);
  } else {
    drawBigTime("00:00", 78, DIM);
    spr.setTextColor(DIM, TFT_BLACK);
    spr.drawString("no NTP - tieni KEY al boot", SCR_W/2, 150, 2);
  }
}

void drawTimer() {
  char buf[32];
  bool pomo = (mode == MODE_POMODORO);
  spr.setTextDatum(MC_DATUM);

  // fase pomodoro: etichetta discreta in alto
  if (pomo) {
    spr.setTextColor(DIM, TFT_BLACK);
    spr.drawString(phaseName(pomoPhase), SCR_W/2, 30, 2);
  }

  // tempo grande bianco (countdown pomodoro, crescente cronometro)
  if (pomo) {
    uint32_t e = elapsedMs();
    uint32_t rem = (e >= pomoTargetMs) ? 0 : (pomoTargetMs - e);
    fmtHMS(rem, buf, sizeof(buf));
  } else {
    fmtHMS(elapsedMs(), buf, sizeof(buf));
  }
  drawBigTime(buf, 82, TFT_WHITE);

  // barra progresso pomodoro: linea sottile discreta
  if (pomo) {
    const int bw = 240, bx = SCR_W/2 - bw/2, by = 138;
    spr.drawFastHLine(bx, by, bw, tft.color565(50, 50, 50));
    uint32_t e = elapsedMs();
    if (pomoTargetMs > 0) {
      uint32_t cap = e > pomoTargetMs ? pomoTargetMs : e;
      int fw = (int)((uint64_t)bw * cap / pomoTargetMs);
      if (fw > 0) spr.drawFastHLine(bx, by, fw, TFT_WHITE);
    }
  }

  // stato discreto in basso
  const char* st = runSt == ST_RUNNING ? "RUNNING"
                 : runSt == ST_PAUSED  ? "PAUSA" : "PRONTO";
  spr.setTextColor(DIM, TFT_BLACK);
  spr.drawString(st, SCR_W/2, 148, 2);

  // contatore cicli pomodoro discreto
  if (pomo) {
    snprintf(buf, sizeof(buf), "%d/%d", pomoDoneWork % cfgCycles, cfgCycles);
    spr.setTextDatum(BR_DATUM);
    spr.setTextColor(DIM, TFT_BLACK);
    spr.drawString(buf, SCR_W-6, SCR_H-4, 2);
  }
}

// pagina INFO (long-press BOOT): batteria, ricarica, IP
bool     showInfo  = false;
uint32_t infoSince = 0;

void drawInfo() {
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(DIM, TFT_BLACK);
  spr.drawString("INFO", 6, 6, 2);
  // revisione firmware in alto a destra
  spr.setTextDatum(TR_DATUM);
  spr.drawString("v" FW_VERSION, SCR_W - 6, 6, 2);
  spr.setTextDatum(TL_DATUM);

  int mv = batteryMv();
  bool present = batteryPresent(mv);
  int pct = (mv - BAT_MV_EMPTY) * 100 / (BAT_MV_FULL - BAT_MV_EMPTY);
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  bool charging = present && mv >= BAT_MV_FULL;

  char buf[32];
  // batteria: percentuale grande a sinistra
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.setTextDatum(TL_DATUM);
  if (present) { snprintf(buf, sizeof(buf), "%d%%", pct); spr.drawString(buf, 6, 36, 7); }
  else         { spr.drawString("USB", 6, 36, 7); }

  // tensione sotto
  snprintf(buf, sizeof(buf), "%d.%02d V", mv / 1000, (mv % 1000) / 10);
  spr.setTextColor(DIM, TFT_BLACK);
  spr.drawString(buf, 6, 110, 4);

  // ricarica a destra
  spr.setTextColor(DIM, TFT_BLACK);
  spr.drawString("Ricarica", 196, 40, 2);
  spr.setTextColor(charging ? TFT_GREEN : DIM, TFT_BLACK);
  spr.drawString(charging ? "SI" : "NO", 196, 58, 6);

  // IP in basso
  spr.setTextColor(DIM, TFT_BLACK);
  spr.setTextDatum(BL_DATUM);
  spr.drawString(wifiUp ? ipStr.c_str() : "WiFi off", 6, SCR_H - 4, 4);

  spr.pushSprite(0, 0);
}

void render() {
  if (showInfo) { drawInfo(); return; }

  spr.fillSprite(TFT_BLACK);

  // header discreto: nome modo
  spr.setTextColor(DIM, TFT_BLACK);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(modeName(mode), 6, 6, 2);

  // indicatore NTP discreto in alto a destra
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(timeSynced ? DIM : 0x4208, TFT_BLACK);
  spr.drawString(timeSynced ? "NTP" : "---", SCR_W-6, 6, 2);

  if (mode == MODE_CLOCK) {
    drawClock();
    // numero sessioni loggate (discreto)
    char buf[24];
    snprintf(buf, sizeof(buf), "log %d", countSessions());
    spr.setTextColor(DIM, TFT_BLACK);
    spr.setTextDatum(BL_DATUM);
    spr.drawString(buf, 6, SCR_H-4, 2);
    // IP per web config (discreto, in basso a destra)
    if (wifiUp) {
      spr.setTextDatum(BR_DATUM);
      spr.drawString(ipStr, SCR_W-6, SCR_H-4, 2);
    }
  } else {
    drawTimer();
  }

  spr.pushSprite(0, 0);
}

// ---------------- WiFi / NTP (portale captive) ----------------
// disegna istruzioni quando si apre il portale di configurazione
void onPortal(WiFiManager *wm) {
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.drawString("CONFIG WiFi", 6, 6, 4);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString("1. WiFi telefono:", 6, 50, 2);
  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.drawString(AP_NAME, 6, 70, 4);
  if (strlen(AP_PASS) > 0) {
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString("pass: " AP_PASS, 6, 100, 2);
  }
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString("2. Apri portale, scegli rete", 6, 124, 2);
  spr.drawString("3. Salva. Timeout 120s.", 6, 144, 2);
  spr.pushSprite(0, 0);
}

void setupTime(bool forcePortal) {
  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setAPCallback(onPortal);

  bool ok;
  if (forcePortal) {
    Serial.println("[WiFi] portale forzato");
    ok = wm.startConfigPortal(AP_NAME, strlen(AP_PASS) ? AP_PASS : nullptr);
  } else {
    // autoConnect: usa creds salvate, altrimenti apre portale
    ok = wm.autoConnect(AP_NAME, strlen(AP_PASS) ? AP_PASS : nullptr);
  }

  if (ok && WiFi.status() == WL_CONNECTED) {
    ipStr  = WiFi.localIP().toString();
    wifiUp = true;
    Serial.printf("[WiFi] ok %s\n", ipStr.c_str());
    configTime(cfgGmtMin * 60, cfgDstMin * 60, NTP_SERVER);
    struct tm tmv;
    if (getLocalTime(&tmv, 5000)) { timeSynced = true; Serial.println("[NTP] sync ok"); }
    // WiFi resta acceso per il web config (vedi startWeb)
  } else {
    Serial.println("[WiFi] no connessione");
    WiFi.mode(WIFI_OFF);
  }
}

// ---------------- Web dashboard ----------------
// Pagina unica autonoma (nessun asset esterno). Stato live via /status,
// log sessioni via /log; le etichette IT/EN sono gestite client-side.
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html><html lang=it><head>
<meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<meta name=color-scheme content=dark><title>WorkTimer</title><style>
:root{
--bg:oklch(0.17 0.012 150);--panel:oklch(0.21 0.012 150);--line:oklch(0.30 0.010 150);
--digit:oklch(0.95 0.025 150);--label:oklch(0.63 0.015 150);--ghost:oklch(0.295 0.012 150);
--accent:oklch(0.80 0.17 152);--accent-ink:oklch(0.20 0.04 152);
--work:oklch(0.66 0.18 25);--break:oklch(0.80 0.16 152);--long:oklch(0.72 0.13 250);}
*{box-sizing:border-box}html,body{margin:0}
body{background:var(--bg);color:var(--digit);
font-family:"SF Mono","DejaVu Sans Mono",Consolas,ui-monospace,monospace;
-webkit-font-smoothing:antialiased;padding:22px 18px 44px;max-width:440px;margin:0 auto;
transition:opacity .2s}
body.off{opacity:.45}
header{display:flex;justify-content:space-between;align-items:center;margin-bottom:26px}
.logo{font-size:13px;letter-spacing:.28em;color:var(--label);text-transform:uppercase}
#lang{background:none;border:1px solid var(--line);color:var(--label);font:inherit;font-size:12px;
padding:6px 12px;border-radius:7px;min-height:34px;cursor:pointer;transition:border-color .15s,color .15s}
#lang:hover{border-color:var(--label);color:var(--digit)}
h2{font-size:11px;letter-spacing:.24em;text-transform:uppercase;color:var(--label);font-weight:400;margin:0 0 16px}
.hero{position:relative;text-align:center;line-height:1;margin:6px 0 18px}
.hero .ghost,.hero .time{font-size:clamp(58px,19vw,104px);font-weight:700;
font-variant-numeric:tabular-nums;letter-spacing:.02em}
.hero .ghost{position:absolute;inset:0;color:var(--ghost);user-select:none}
.hero .time{position:relative;color:var(--digit)}
.meta{display:flex;align-items:center;justify-content:center;gap:12px;flex-wrap:wrap;margin-bottom:14px}
#mode{font-size:12px;letter-spacing:.22em;text-transform:uppercase;color:var(--label)}
.phase{font-size:12px;letter-spacing:.16em;text-transform:uppercase}
.ph0{color:var(--work)}.ph1{color:var(--break)}.ph2{color:var(--long)}.ph-none{display:none}
.pill{font-size:11px;letter-spacing:.14em;text-transform:uppercase;padding:5px 11px;border-radius:999px;
border:1px solid var(--line);color:var(--label)}
.st1{background:var(--accent);color:var(--accent-ink);border-color:var(--accent);font-weight:600}
.st2{border-color:var(--label);color:var(--digit)}
.dots{display:flex;justify-content:center;gap:8px;min-height:11px;margin-bottom:13px}
.dot{width:9px;height:9px;border-radius:50%;border:1px solid var(--line)}
.dot.on{background:var(--accent);border-color:var(--accent)}
.ip{text-align:center;font-size:11px;color:var(--label);letter-spacing:.04em}
hr{border:0;border-top:1px solid var(--line);margin:30px 0}
.field{margin-bottom:16px}
label{display:block;font-size:12px;color:var(--label);margin:0 0 6px;letter-spacing:.02em}
input{width:100%;background:var(--panel);border:1px solid var(--line);color:var(--digit);font:inherit;
font-size:18px;font-variant-numeric:tabular-nums;padding:11px 13px;border-radius:9px;min-height:46px}
input:focus{outline:none;border-color:var(--accent)}
#save{margin-top:6px;width:100%;background:var(--accent);color:var(--accent-ink);border:0;font:inherit;
font-size:15px;font-weight:600;letter-spacing:.05em;text-transform:uppercase;padding:13px;border-radius:9px;
min-height:48px;cursor:pointer;transition:filter .12s}
#save:active{filter:brightness(.92)}
.saved{display:block;text-align:center;font-size:12px;color:var(--accent);margin-top:12px;opacity:0;
letter-spacing:.1em;text-transform:uppercase;transition:opacity .25s}
.saved.show{opacity:1}
.grp{margin-bottom:26px}
.grp-h{font-size:10px;letter-spacing:.2em;text-transform:uppercase;color:var(--label);opacity:.72;
margin:0 0 13px;padding-bottom:7px;border-bottom:1px solid var(--line)}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px}
.row2:last-child{margin-bottom:0}
.row2 .field{margin:0}
.grp .check:first-of-type{margin-top:12px}
.totals{display:flex;gap:32px;margin-bottom:18px}
.totals>div span{font-size:11px;letter-spacing:.18em;text-transform:uppercase;color:var(--label)}
.totals b{display:block;font-size:24px;font-weight:600;color:var(--digit);font-variant-numeric:tabular-nums;
letter-spacing:.02em;margin-top:5px}
#rows{list-style:none;margin:0;padding:0}
#rows li{display:flex;align-items:baseline;gap:14px;padding:9px 0;border-top:1px solid var(--line);font-size:13px}
#rows li:first-child{border-top:0}
.lt{color:var(--label);font-variant-numeric:tabular-nums;min-width:42px}
.lk{flex:1;color:var(--digit)}.ls{color:var(--label);font-variant-numeric:tabular-nums}
#empty{color:var(--label);font-size:13px;margin:4px 0 0}
.controls{display:flex;gap:8px;margin:4px 0 14px}
.ctl{flex:1;background:var(--accent);color:var(--accent-ink);border:0;font:inherit;font-size:13px;
font-weight:600;text-transform:uppercase;letter-spacing:.04em;padding:11px;border-radius:9px;min-height:44px;cursor:pointer}
.ctl:disabled{opacity:.4;cursor:default}
.ghostbtn{background:none;border:1px solid var(--line);color:var(--label)}
.ghostbtn:hover{border-color:var(--label);color:var(--digit)}
.bat{text-align:center;font-size:11px;color:var(--label);letter-spacing:.04em;margin-top:6px}
.check{display:flex;align-items:center;gap:10px;font-size:12px;color:var(--label);margin:2px 0 4px;cursor:pointer}
.check input{width:18px;height:18px;accent-color:var(--accent)}
input[type=range]{padding:0;height:34px;accent-color:var(--accent)}
.week{display:flex;align-items:flex-end;gap:6px;height:62px;margin-bottom:20px}
.week .col{flex:1;display:flex;flex-direction:column;align-items:center;gap:5px;height:100%;justify-content:flex-end}
.week .bbar{width:100%;background:var(--accent);border-radius:3px 3px 0 0;min-height:2px;opacity:.85}
.week .bbar.z{background:var(--line);opacity:1}
.week .dl{font-size:9px;color:var(--label)}
.tools{display:flex;gap:10px;flex-wrap:wrap}
.tools a,.tools button{flex:1;text-align:center;background:none;border:1px solid var(--line);color:var(--label);
font:inherit;font-size:12px;letter-spacing:.06em;text-transform:uppercase;padding:10px;border-radius:8px;min-height:42px;
cursor:pointer;text-decoration:none;display:flex;align-items:center;justify-content:center;transition:border-color .15s,color .15s}
.tools a:hover,.tools button:hover{border-color:var(--label);color:var(--digit)}
.note{position:fixed;left:50%;top:16px;width:calc(100% - 36px);max-width:404px;
background:var(--panel);border:1px solid var(--accent);color:var(--digit);border-radius:10px;
padding:13px 16px;font-size:13px;letter-spacing:.02em;text-align:center;z-index:10;
opacity:0;pointer-events:none;transform:translateX(-50%) translateY(-8px);transition:opacity .25s,transform .25s}
.note.show{opacity:1;transform:translateX(-50%) translateY(0)}
@media (prefers-reduced-motion:reduce){*{transition:none!important;animation:none!important}}
</style></head><body>
<header><span class=logo>WorkTimer</span><button id=lang aria-label="switch language">EN</button></header>
<div id=note class=note role=status aria-live=polite></div>
<section aria-live=polite>
<h2 id=h-status>Stato</h2>
<div class=hero><div class=ghost id=ghost>88:88</div><div class=time id=time>--:--</div></div>
<div class=meta><span class=pill id=state></span><span id=mode></span><span class=phase id=phase></span></div>
<div class=dots id=dots></div>
<div class=controls>
<button id=c-primary class=ctl></button>
<button id=c-stop class="ctl ghostbtn"></button>
<button id=c-mode class="ctl ghostbtn"></button>
</div>
<div class=ip id=ip></div>
<div class=bat id=bat></div>
</section>
<hr>
<section>
<h2 id=h-config>Configurazione</h2>
<div class=grp>
<div class=grp-h id=g-pomo>Pomodoro</div>
<div class=row2>
<div class=field><label id=l-work for=work>Lavoro</label><input id=work type=number min=1 max=180 inputmode=numeric></div>
<div class=field><label id=l-brk for=brk>Pausa</label><input id=brk type=number min=1 max=120 inputmode=numeric></div>
</div>
<div class=row2>
<div class=field><label id=l-long for=long>Pausa lunga</label><input id=long type=number min=1 max=120 inputmode=numeric></div>
<div class=field><label id=l-cyc for=cyc>Cicli</label><input id=cyc type=number min=1 max=12 inputmode=numeric></div>
</div>
</div>
<div class=grp>
<div class=grp-h id=g-time>Tempo</div>
<div class=row2>
<div class=field><label id=l-gmt for=gmt>Fuso</label><input id=gmt type=number min=-12 max=14 inputmode=numeric></div>
<div class=field><label id=l-dst for=dst>Ora legale</label><input id=dst type=number min=0 max=2 inputmode=numeric></div>
</div>
</div>
<div class=grp>
<div class=grp-h id=g-disp>Display</div>
<div class=field><label id=l-bright for=bright>Luminosita</label><input id=bright type=range min=10 max=255></div>
<label class=check for=auto><input id=auto type=checkbox><span id=l-auto>Avanzamento automatico</span></label>
<label class=check for=buzz><input id=buzz type=checkbox><span id=l-buzz>Buzzer fine pomodoro</span></label>
</div>
<button id=save>Salva</button><span class=saved id=saved></span>
</section>
<hr>
<section>
<h2 id=h-sessions>Sessioni</h2>
<div class=totals><div><span id=t-today>Oggi</span><b id=today>--:--</b></div><div><span id=t-total>Totale</span><b id=total>--:--</b></div><div><span id=t-pomos>Pomodori</span><b id=pomos>0</b></div></div>
<div class=week id=week></div>
<ul id=rows></ul><p id=empty></p>
<div class=tools><a id=t-csv href="/sessions.csv" download>CSV</a><button id=t-clear class=ghostbtn></button></div>
</section>
<hr>
<section>
<h2 id=h-system>Sistema</h2>
<div class=tools><a id=t-ota href="/update">OTA</a><button id=t-wifi class=ghostbtn></button></div>
</section>
<script>
const T={
it:{status:"Stato",config:"Configurazione",sessions:"Sessioni",system:"Sistema",
gpomo:"Pomodoro",gtime:"Tempo",gdisp:"Display",work:"Lavoro",brk:"Pausa",
long:"Pausa lunga",cyc:"Cicli prima della pausa lunga",gmt:"Fuso orario (h)",dst:"Ora legale (+h)",
bright:"Luminosita",auto:"Avanzamento automatico",buzz:"Buzzer fine pomodoro",save:"Salva",saved:"Salvato",min:"min",
today:"Oggi",total:"Totale",pomos:"Pomodori",empty:"Nessuna sessione registrata",nontp:"orario non sincronizzato",
csv:"CSV",clear:"Cancella log",ota:"Firmware",wifioff:"Spegni WiFi",charging:"in carica",
start:"Avvia",pause:"Pausa",resume:"Riprendi",stop:"Stop",modeBtn:"Modo",
askClear:"Cancellare tutto lo storico delle sessioni?",
askWifi:"Spegnere il WiFi? La pagina non sara' piu' raggiungibile finche' non riavvii tenendo premuto KEY.",
noteWork:"Lavoro: si parte",noteBreak:"Pausa: stacca un attimo",noteLong:"Pausa lunga: ben fatto",
modes:["Orologio","Cronometro","Pomodoro"],phases:["Lavoro","Pausa","Pausa lunga"],
states:["Fermo","In corso","In pausa"],
kinds:{"pomodoro-work":"Lavoro","pomodoro-partial":"Parziale","stopwatch":"Cronometro"}},
en:{status:"Status",config:"Configuration",sessions:"Sessions",system:"System",
gpomo:"Pomodoro",gtime:"Time",gdisp:"Display",work:"Work",brk:"Break",
long:"Long break",cyc:"Cycles before long break",gmt:"Timezone (h)",dst:"DST (+h)",
bright:"Brightness",auto:"Auto-advance",buzz:"Buzzer at pomodoro end",save:"Save",saved:"Saved",min:"min",
today:"Today",total:"Total",pomos:"Pomodoros",empty:"No sessions yet",nontp:"clock not synced",
csv:"CSV",clear:"Clear log",ota:"Firmware",wifioff:"WiFi off",charging:"charging",
start:"Start",pause:"Pause",resume:"Resume",stop:"Stop",modeBtn:"Mode",
askClear:"Clear the whole session history?",
askWifi:"Turn WiFi off? The page won't be reachable until you reboot holding KEY.",
noteWork:"Work: go",noteBreak:"Break: step away",noteLong:"Long break: well done",
modes:["Clock","Stopwatch","Pomodoro"],phases:["Work","Break","Long break"],
states:["Idle","Running","Paused"],
kinds:{"pomodoro-work":"Work","pomodoro-partial":"Partial","stopwatch":"Stopwatch"}}};
const $=id=>document.getElementById(id);
let lang=localStorage.getItem("wt_lang")||"it",cfgLoaded=false,last=null,prevPhase=null,prevMode=null,noteTimer;
function fmt(s){s=Math.max(0,Math.floor(s));let h=(s/3600)|0;s%=3600;let m=(s/60)|0,x=s%60;
const p=n=>String(n).padStart(2,"0");return h>0?h+":"+p(m)+":"+p(x):p(m)+":"+p(x);}
function hero(str){$("time").textContent=str;$("ghost").textContent=str.replace(/\d/g,"8");}
function notify(msg){const n=$("note");n.textContent=msg;n.classList.add("show");
clearTimeout(noteTimer);noteTimer=setTimeout(()=>n.classList.remove("show"),5000);
try{if(window.Notification&&Notification.permission==="granted")new Notification("WorkTimer",{body:msg});}catch(e){}}
function askNotify(){try{if(window.Notification&&Notification.permission==="default")Notification.requestPermission();}catch(e){}}
function applyStatic(){const d=T[lang];document.documentElement.lang=lang;
$("lang").textContent=lang=="it"?"EN":"IT";
$("h-status").textContent=d.status;$("h-config").textContent=d.config;$("h-sessions").textContent=d.sessions;$("h-system").textContent=d.system;
$("g-pomo").textContent=d.gpomo;$("g-time").textContent=d.gtime;$("g-disp").textContent=d.gdisp;
$("l-work").textContent=d.work+" ("+d.min+")";$("l-brk").textContent=d.brk+" ("+d.min+")";
$("l-long").textContent=d.long+" ("+d.min+")";$("l-cyc").textContent=d.cyc;
$("l-gmt").textContent=d.gmt;$("l-dst").textContent=d.dst;$("l-bright").textContent=d.bright;$("l-auto").textContent=d.auto;$("l-buzz").textContent=d.buzz;
$("save").textContent=d.save;$("t-today").textContent=d.today;$("t-total").textContent=d.total;$("t-pomos").textContent=d.pomos;
$("t-csv").textContent=d.csv;$("t-clear").textContent=d.clear;$("t-ota").textContent=d.ota;$("t-wifi").textContent=d.wifioff;
$("c-stop").textContent=d.stop;$("c-mode").textContent=d.modeBtn;
if(last)render(last);}
function render(s){const d=T[lang];let str;
if(s.mode===0){const n=new Date();str=String(n.getHours()).padStart(2,"0")+":"+String(n.getMinutes()).padStart(2,"0");}
else if(s.mode===2){str=fmt(s.target-s.elapsed);}else{str=fmt(s.elapsed);}
hero(str);
$("mode").textContent=d.modes[s.mode]||"";
const ph=$("phase");ph.textContent=s.mode===2?(d.phases[s.phase]||""):"";
ph.className="phase "+(s.mode===2?"ph"+s.phase:"ph-none");
const st=$("state");st.textContent=d.states[s.state]||"";st.className="pill st"+s.state;
const dots=$("dots");dots.textContent="";
if(s.mode===2&&s.cycles>0){const done=s.done%s.cycles;
for(let i=0;i<s.cycles;i++){const e=document.createElement("span");e.className="dot"+(i<done?" on":"");dots.appendChild(e);}}
const cp=$("c-primary");cp.disabled=s.mode===0;
cp.textContent=s.state===1?d.pause:(s.state===2?d.resume:d.start);
$("c-stop").disabled=s.state===0;$("c-mode").disabled=s.state!==0;
$("ip").textContent="worktimer.local · "+(s.ip||"")+(s.fw?" · v"+s.fw:"")+(s.synced?"":" · "+d.nontp);
if(s.batmv>0){let p=Math.round((s.batmv-3300)/9);p=Math.max(0,Math.min(100,p));
$("bat").textContent=p+"% · "+(s.batmv/1000).toFixed(2)+" V"+(s.charging?" · "+d.charging:"");}else $("bat").textContent="";
if(s.mode===2&&prevMode===2&&prevPhase!==null&&s.phase!==prevPhase)
notify(s.phase===0?d.noteWork:(s.phase===2?d.noteLong:d.noteBreak));
prevPhase=s.phase;prevMode=s.mode;
if(!cfgLoaded){$("work").value=s.cwork;$("brk").value=s.cbreak;$("long").value=s.clong;$("cyc").value=s.cycles;
$("gmt").value=s.cgmt;$("dst").value=s.cdst;$("bright").value=s.cbright;$("auto").checked=s.cauto;$("buzz").checked=s.cbuzz;cfgLoaded=true;}}
async function poll(){try{const r=await fetch("/status",{cache:"no-store"});if(!r.ok)throw 0;
last=await r.json();document.body.classList.remove("off");render(last);}
catch(e){document.body.classList.add("off");}}
async function loadLog(){try{const r=await fetch("/log",{cache:"no-store"});const j=await r.json(),d=T[lang];
$("today").textContent=fmt(j.today);$("total").textContent=fmt(j.total);$("pomos").textContent=j.pomos;
const wk=$("week");wk.textContent="";const mx=Math.max(1,...j.days.map(x=>x.s));
j.days.forEach(x=>{const col=document.createElement("div");col.className="col";
const bar=document.createElement("div");bar.className="bbar"+(x.s?"":" z");
bar.style.height=x.s?Math.max(4,Math.round(x.s/mx*100))+"%":"2px";
const lbl=document.createElement("div");lbl.className="dl";
lbl.textContent=x.d?new Date(x.d+"T00:00").toLocaleDateString(lang,{weekday:"short"}).slice(0,2):"";
col.append(bar,lbl);wk.appendChild(col);});
const ul=$("rows");ul.textContent="";
if(!j.rows.length){$("empty").textContent=d.empty;$("empty").style.display="block";}
else{$("empty").style.display="none";j.rows.forEach(row=>{const li=document.createElement("li");
const tm=row.t&&row.t!="no-time"?row.t.slice(11,16):"—",k=d.kinds[row.k]||row.k;
const a=document.createElement("span");a.className="lt";a.textContent=tm;
const b=document.createElement("span");b.className="lk";b.textContent=k;
const c=document.createElement("span");c.className="ls";c.textContent=fmt(row.s);
li.append(a,b,c);ul.appendChild(li);});}}catch(e){}}
async function cmd(a){askNotify();try{await fetch("/cmd?a="+a,{cache:"no-store"});poll();}catch(e){}}
$("c-primary").onclick=()=>cmd("start");
$("c-stop").onclick=()=>cmd("stop");
$("c-mode").onclick=()=>cmd("mode");
$("save").addEventListener("click",async()=>{askNotify();
const body=new URLSearchParams({work:$("work").value,brk:$("brk").value,long:$("long").value,cyc:$("cyc").value,
gmt:$("gmt").value,dst:$("dst").value,bright:$("bright").value,auto:$("auto").checked?"1":"0",buzz:$("buzz").checked?"1":"0"});
try{await fetch("/save",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body});
const sv=$("saved");sv.textContent=T[lang].saved;sv.classList.add("show");
setTimeout(()=>sv.classList.remove("show"),1600);}catch(e){}});
$("t-clear").onclick=async()=>{if(!confirm(T[lang].askClear))return;
try{await fetch("/clearlog",{method:"POST"});loadLog();}catch(e){}};
$("t-wifi").onclick=async()=>{if(!confirm(T[lang].askWifi))return;
try{await fetch("/wifioff",{method:"POST"});}catch(e){}document.body.classList.add("off");};
$("lang").addEventListener("click",()=>{lang=lang=="it"?"en":"it";localStorage.setItem("wt_lang",lang);applyStatic();loadLog();});
applyStatic();poll();loadLog();setInterval(poll,1000);setInterval(loadLog,15000);
</script></body></html>)HTML";

String htmlPage() { return FPSTR(PAGE_HTML); }

void handleRoot() { server.send_P(200, "text/html", PAGE_HTML); }

// stato live per il polling del dashboard
String statusJson() {
  String j = "{";
  j += "\"mode\":" + String((int)mode);
  j += ",\"state\":" + String((int)runSt);
  j += ",\"phase\":" + String((int)pomoPhase);
  j += ",\"elapsed\":" + String(elapsedMs() / 1000);
  j += ",\"target\":" + String(pomoTargetMs / 1000);
  j += ",\"done\":" + String(pomoDoneWork);
  j += ",\"cycles\":" + String(cfgCycles);
  j += ",\"cwork\":" + String(cfgWork);
  j += ",\"cbreak\":" + String(cfgBreak);
  j += ",\"clong\":" + String(cfgLong);
  j += ",\"cgmt\":" + String(cfgGmtMin / 60);
  j += ",\"cdst\":" + String(cfgDstMin / 60);
  j += ",\"cauto\":"; j += (cfgAutoAdv ? "true" : "false");
  j += ",\"cbright\":" + String(cfgBright);
  j += ",\"cbuzz\":"; j += (cfgBuzz ? "true" : "false");
  int bmv = batteryMv();
  j += ",\"batmv\":" + String(bmv);
  j += ",\"charging\":"; j += (bmv >= BAT_MV_FULL ? "true" : "false");
  j += ",\"synced\":"; j += (timeSynced ? "true" : "false");
  j += ",\"fw\":\"" FW_VERSION "\"";
  j += ",\"ip\":\"" + ipStr + "\"}";
  return j;
}
void handleStatus() { server.send(200, "application/json", statusJson()); }

// log sessioni: ultime 50 righe (piu' recenti prima) + totali oggi/complessivo
String logJson() {
  uint32_t total = 0, today = 0;
  int pomosToday = 0;
  char dbuf[11] = "";
  String dates[7];          // ultimi 7 giorni, dal piu' vecchio (0) a oggi (6)
  uint32_t days[7] = {0};
  if (timeSynced) {
    time_t now = time(nullptr);
    struct tm tmv; localtime_r(&now, &tmv);
    strftime(dbuf, sizeof(dbuf), "%Y-%m-%d", &tmv);
    for (int i = 0; i < 7; i++) {
      time_t d = now - (time_t)(6 - i) * 86400;
      struct tm td; localtime_r(&d, &td);
      char db[11]; strftime(db, sizeof(db), "%Y-%m-%d", &td);
      dates[i] = db;
    }
  }
  std::vector<String> lines;
  if (fsReady) {
    File f = LittleFS.open(LOG_PATH, FILE_READ);
    if (f) {
      String line;
      while (f.available()) {
        char c = (char)f.read();
        if (c == '\n') { if (line.length()) lines.push_back(line); line = ""; }
        else if (c != '\r') line += c;
      }
      if (line.length()) lines.push_back(line);
      f.close();
    }
  }
  for (auto &ln : lines) {
    int c1 = ln.indexOf(','); int c2 = ln.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) continue;
    uint32_t sec = (uint32_t)ln.substring(c2 + 1).toInt();
    String date = ln.substring(0, 10);
    String kind = ln.substring(c1 + 1, c2);
    total += sec;
    if (dbuf[0] && date == dbuf) {
      today += sec;
      if (kind == "pomodoro-work") pomosToday++;
    }
    for (int i = 0; i < 7; i++) if (dates[i].length() && date == dates[i]) { days[i] += sec; break; }
  }
  String j = "{\"rows\":[";
  int start = lines.size() > 50 ? (int)lines.size() - 50 : 0;
  bool first = true;
  for (int i = (int)lines.size() - 1; i >= start; i--) {
    String &ln = lines[i];
    int c1 = ln.indexOf(','); int c2 = ln.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"t\":\"" + ln.substring(0, c1) + "\",\"k\":\"" + ln.substring(c1 + 1, c2)
       + "\",\"s\":" + ln.substring(c2 + 1) + "}";
  }
  j += "],\"today\":" + String(today) + ",\"total\":" + String(total)
     + ",\"count\":" + String((int)lines.size()) + ",\"pomos\":" + String(pomosToday)
     + ",\"days\":[";
  for (int i = 0; i < 7; i++) {
    if (i) j += ",";
    j += "{\"d\":\"" + dates[i] + "\",\"s\":" + String(days[i]) + "}";
  }
  j += "]}";
  return j;
}
void handleLog() { server.send(200, "application/json", logJson()); }

int clampInt(const String& s, int lo, int hi, int def) {
  if (s.length() == 0) return def;
  int v = s.toInt();
  if (v < lo) v = lo; if (v > hi) v = hi;
  return v;
}

void handleSave() {
  cfgWork   = clampInt(server.arg("work"), 1, 180, cfgWork);
  cfgBreak  = clampInt(server.arg("brk"),  1, 120, cfgBreak);
  cfgLong   = clampInt(server.arg("long"), 1, 120, cfgLong);
  cfgCycles = clampInt(server.arg("cyc"),  1, 12,  cfgCycles);
  cfgGmtMin = clampInt(server.arg("gmt"), -12, 14, cfgGmtMin / 60) * 60;
  cfgDstMin = clampInt(server.arg("dst"),   0,  2, cfgDstMin / 60) * 60;
  cfgBright = clampInt(server.arg("bright"), 10, 255, cfgBright);
  if (server.hasArg("auto")) cfgAutoAdv = (server.arg("auto") == "1");
  if (server.hasArg("buzz")) cfgBuzz    = (server.arg("buzz") == "1");
  saveConfig();
  applyBright();
  if (wifiUp) configTime(cfgGmtMin * 60, cfgDstMin * 60, NTP_SERVER);
  // applica subito se pomodoro fermo (no interruzione sessione attiva)
  if (mode == MODE_POMODORO && runSt == ST_IDLE) pomoSetPhase(pomoPhase);
  Serial.printf("[CFG] work=%d brk=%d long=%d cyc=%d gmt=%d dst=%d bright=%d auto=%d\n",
                cfgWork, cfgBreak, cfgLong, cfgCycles, cfgGmtMin / 60, cfgDstMin / 60, cfgBright, cfgAutoAdv);
  server.send(200, "application/json", "{\"ok\":true}");
}

// comandi remoti: replica i bottoni fisici
void handleCmd() {
  String a = server.arg("a");
  if      (a == "start" || a == "pause" || a == "resume") actionPrimary();
  else if (a == "stop")  actionStop();
  else if (a == "mode")  actionModeSwitch();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleClearLog() {
  if (fsReady) LittleFS.remove(LOG_PATH);
  Serial.println("[LOG] reset via web");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleCsv() {
  if (!fsReady || !LittleFS.exists(LOG_PATH)) { server.send(204, "text/plain", ""); return; }
  File f = LittleFS.open(LOG_PATH, FILE_READ);
  server.sendHeader("Content-Disposition", "attachment; filename=sessions.csv");
  server.streamFile(f, "text/csv");
  f.close();
}

// spegne il WiFi per risparmio batteria. Riaccensione: tieni KEY all'avvio.
void handleWifiOff() {
  server.send(200, "application/json", "{\"ok\":true}");
  delay(150);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiUp = false;
  Serial.println("[WiFi] spento via web (riaccendi: tieni KEY all'avvio)");
}

// ---- OTA: aggiornamento firmware via browser ----
static const char OTA_HTML[] PROGMEM = R"HTML(<!doctype html><html lang=it><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><meta name=color-scheme content=dark>
<title>WorkTimer · OTA</title><style>
body{background:oklch(0.17 0.012 150);color:oklch(0.95 0.025 150);
font-family:"SF Mono","DejaVu Sans Mono",Consolas,ui-monospace,monospace;max-width:440px;margin:0 auto;padding:28px 18px}
a{color:oklch(0.80 0.17 152);text-decoration:none;font-size:12px}
h1{font-size:13px;letter-spacing:.24em;text-transform:uppercase;color:oklch(0.63 0.015 150);font-weight:400;margin:18px 0}
input[type=file]{width:100%;margin:0 0 18px;color:oklch(0.63 0.015 150);font:inherit;font-size:13px}
button{width:100%;background:oklch(0.80 0.17 152);color:oklch(0.20 0.04 152);border:0;font:inherit;font-size:15px;
font-weight:600;text-transform:uppercase;letter-spacing:.05em;padding:13px;border-radius:9px;cursor:pointer}
#bar{height:6px;background:oklch(0.30 0.01 150);border-radius:3px;margin-top:18px;overflow:hidden}
#fill{height:100%;width:0;background:oklch(0.80 0.17 152);transition:width .2s}
#msg{font-size:12px;color:oklch(0.63 0.015 150);margin-top:12px;min-height:16px;letter-spacing:.04em}
</style></head><body><a href="/">&larr; dashboard</a><h1>Firmware update</h1>
<input id=f type=file accept=.bin><button id=go>Carica / Upload</button>
<div id=bar><div id=fill></div></div><div id=msg>firmware.bin da build/lilygo-t-display-s3/</div>
<script>const $=i=>document.getElementById(i);
$("go").onclick=()=>{const f=$("f").files[0];if(!f){$("msg").textContent="seleziona un .bin";return;}
const fd=new FormData();fd.append("firmware",f,f.name);const x=new XMLHttpRequest();x.open("POST","/update");
x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);$("fill").style.width=p+"%";$("msg").textContent=p+"%";}};
x.onload=()=>{const ok=x.status==200;$("msg").textContent=ok?"OK, riavvio…":"errore "+x.status;if(ok)setTimeout(()=>location.href="/",6000);};
x.onerror=()=>$("msg").textContent="errore di rete";x.send(fd);};
</script></body></html>)HTML";

void handleUpdatePage() { server.send_P(200, "text/html", OTA_HTML); }

void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] start %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("[OTA] ok %u byte\n", up.totalSize);
    else Update.printError(Serial);
  }
}

void startWeb() {
  if (!wifiUp) return;
  if (MDNS.begin("worktimer")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[WEB] http://worktimer.local");
  }
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/cmd", HTTP_GET, handleCmd);
  server.on("/clearlog", HTTP_POST, handleClearLog);
  server.on("/sessions.csv", HTTP_GET, handleCsv);
  server.on("/wifioff", HTTP_POST, handleWifiOff);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", Update.hasError() ? "{\"ok\":false}" : "{\"ok\":true}");
    delay(300); ESP.restart();
  }, handleUpdateUpload);
  server.begin();
  Serial.printf("[WEB] http://%s/\n", ipStr.c_str());
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);     // alimenta LCD/board
  pinMode(PIN_BTN_KEY,  INPUT_PULLUP);
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);

  // ADC batteria (partitore su GPIO4): range pieno per leggere fino a ~4.2V*2
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

  tft.init();
  tft.setRotation(1);                   // landscape 320x170
  tft.invertDisplay(true);              // pannello T-Display S3: corregge colori invertiti
  tft.fillScreen(TFT_BLACK);
  spr.setColorDepth(16);
  if (!spr.createSprite(SCR_W, SCR_H)) {
    Serial.println("[SPR] alloc fail");  // serve PSRAM
  }

  // backlight su PWM (luminosita' regolabile via web)
  ledcSetup(BL_CH, 5000, 8);
  ledcAttachPin(TFT_BL, BL_CH);
  applyBright();

  // buzzer passivo su canale LEDC dedicato (tono variabile)
  if (BUZZER_PIN >= 0) {
    ledcSetup(BUZ_CH, 2000, 8);
    ledcAttachPin(BUZZER_PIN, BUZ_CH);
    ledcWriteTone(BUZ_CH, 0);
  }
  lastActivityMs = millis();

  fsReady = LittleFS.begin(true);
  if (!fsReady) Serial.println("[FS] mount fail");

  // stato persistente
  prefs.begin("worktimer", false);
  loadConfig();
  applyBright();              // applica luminosita' salvata
  loadState();
  pomoSetPhase(pomoPhase);   // imposta pomoTargetMs per la fase ripristinata

  // KEY tenuto all'accensione -> forza portale config WiFi
  // (non uso BOOT/GPIO0: e' strapping pin, tenuto al reset entra in download mode)
  bool forcePortal = (digitalRead(PIN_BTN_KEY) == LOW);
  setupTime(forcePortal);
  startWeb();                // web config pomodoro (se WiFi connesso)

  Serial.println("[BOOT] ready");
}

void loop() {
  // bottoni
  BtnEvent ek = pollButton(bKey);
  BtnEvent eb = pollButton(bBoot);

  // schermo addormentato: qualunque pressione risveglia e viene "ingoiata"
  // (longFired=true impedisce a EV_SHORT/EV_LONG di partire al rilascio)
  if (screenAsleep) {
    bool anyPress = (ek != EV_NONE) || (eb != EV_NONE)
                 || (bKey.stable == LOW) || (bBoot.stable == LOW);
    if (anyPress) { bKey.longFired = true; bBoot.longFired = true; wakeScreen(); }
    if (wifiUp) server.handleClient();
    delay(33);
    return;                       // schermo spento: salto render, risparmio CPU
  }

  // ogni evento tasto conta come attivita' (resetta il timer di sleep)
  if (ek != EV_NONE || eb != EV_NONE) lastActivityMs = millis();

  // BOOT long apre/chiude la pagina INFO; mentre e' visibile una pressione breve la chiude
  if (eb == EV_LONG) { showInfo = !showInfo; infoSince = millis(); }
  else if (showInfo) {
    if (ek == EV_SHORT || eb == EV_SHORT) showInfo = false;
  } else {
    if (ek == EV_SHORT) actionPrimary();
    if (ek == EV_LONG)  actionStop();
    if (eb == EV_SHORT) actionModeSwitch();
  }
  if (showInfo && (millis() - infoSince > 8000)) showInfo = false;   // auto-hide 8s

  // pomodoro: fine fase -> avanza
  if (mode == MODE_POMODORO && runSt == ST_RUNNING) {
    if (elapsedMs() >= pomoTargetMs) {
      bool workEnded = (pomoPhase == PH_WORK);
      // flash schermo come "beep" visivo + beep sonoro
      tft.fillScreen(phaseColor());
      if (cfgBuzz && BUZZER_PIN >= 0) buzzPomodoro(workEnded);  // ~320ms di tono
      else delay(120);                                         // altrimenti flash visivo
      tft.fillScreen(TFT_BLACK);
      pomoAdvance();
    }
  }

  // auto-sleep: idle da troppo -> spegni backlight
  if (SLEEP_IDLE_MS > 0 && !screenAsleep && runSt == ST_IDLE && !showInfo
      && (millis() - lastActivityMs) > SLEEP_IDLE_MS) {
    screenAsleep = true;
    ledcWrite(BL_CH, 0);
  }

  // web config
  if (wifiUp) server.handleClient();

  // salvataggio periodico mentre attivo (recupero dopo calo corrente)
  if (runSt == ST_RUNNING && (millis() - lastStateSave) > STATE_SAVE_MS) {
    saveState();
  }

  render();
  delay(33);   // ~30 fps
}
