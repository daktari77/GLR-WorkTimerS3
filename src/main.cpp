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
#include <Preferences.h>
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
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
}

void saveConfig() {
  prefs.putInt("cwork",  cfgWork);
  prefs.putInt("cbreak", cfgBreak);
  prefs.putInt("clong",  cfgLong);
  prefs.putInt("ccyc",   cfgCycles);
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

// avanza alla fase successiva del pomodoro (chiamato a fine countdown)
void pomoAdvance() {
  if (pomoPhase == PH_WORK) {
    logSession("pomodoro-work", pomoTargetMs);
    pomoDoneWork++;
    if (pomoDoneWork % cfgCycles == 0) pomoSetPhase(PH_LONGBREAK);
    else                                         pomoSetPhase(PH_BREAK);
  } else {
    pomoSetPhase(PH_WORK);
  }
  resetSegment();           // riparte automaticamente la fase nuova
  runSt = ST_RUNNING;
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
  uint8_t font = (strlen(str) <= 5) ? 8 : 7;   // HH:MM/MM:SS = font8, H:MM:SS = font7
  spr.drawString(str, SCR_W/2, cy, font);
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

void render() {
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
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
    struct tm tmv;
    if (getLocalTime(&tmv, 5000)) { timeSynced = true; Serial.println("[NTP] sync ok"); }
    // WiFi resta acceso per il web config (vedi startWeb)
  } else {
    Serial.println("[WiFi] no connessione");
    WiFi.mode(WIFI_OFF);
  }
}

// ---------------- Web config pomodoro ----------------
String htmlPage() {
  String h = F("<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WorkTimer</title><style>"
    "body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:24px}"
    ".c{max-width:360px;margin:auto}h1{font-size:20px;font-weight:600}"
    "label{display:block;margin:14px 0 4px;color:#aaa;font-size:13px}"
    "input{width:100%;padding:10px;border:1px solid #333;border-radius:8px;"
    "background:#1c1c1c;color:#fff;font-size:16px;box-sizing:border-box}"
    "button{margin-top:20px;width:100%;padding:12px;border:0;border-radius:8px;"
    "background:#2d7;color:#000;font-size:16px;font-weight:600}"
    ".m{margin-top:14px;color:#2d7;font-size:14px}</style></head><body><div class='c'>"
    "<h1>Pomodoro config</h1><form method='POST' action='/save'>");
  h += "<label>Lavoro (min)</label><input type='number' min='1' max='180' name='work' value='" + String(cfgWork) + "'>";
  h += "<label>Pausa (min)</label><input type='number' min='1' max='120' name='brk' value='" + String(cfgBreak) + "'>";
  h += "<label>Pausa lunga (min)</label><input type='number' min='1' max='120' name='long' value='" + String(cfgLong) + "'>";
  h += "<label>Cicli prima della pausa lunga</label><input type='number' min='1' max='12' name='cyc' value='" + String(cfgCycles) + "'>";
  h += F("<button type='submit'>Salva</button></form></div></body></html>");
  return h;
}

void handleRoot() { server.send(200, "text/html", htmlPage()); }

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
  saveConfig();
  // applica subito se pomodoro fermo (no interruzione sessione attiva)
  if (mode == MODE_POMODORO && runSt == ST_IDLE) pomoSetPhase(pomoPhase);
  Serial.printf("[CFG] work=%d brk=%d long=%d cyc=%d\n", cfgWork, cfgBreak, cfgLong, cfgCycles);
  String h = F("<!doctype html><meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='1;url=/'>"
    "<body style='font-family:sans-serif;background:#111;color:#2d7;padding:24px'>Salvato.</body>");
  server.send(200, "text/html", h);
}

void startWeb() {
  if (!wifiUp) return;
  if (MDNS.begin("worktimer")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[WEB] http://worktimer.local");
  }
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
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

  tft.init();
  tft.setRotation(1);                   // landscape 320x170
  tft.invertDisplay(true);              // pannello T-Display S3: corregge colori invertiti
  tft.fillScreen(TFT_BLACK);
  spr.setColorDepth(16);
  if (!spr.createSprite(SCR_W, SCR_H)) {
    Serial.println("[SPR] alloc fail");  // serve PSRAM
  }

  fsReady = LittleFS.begin(true);
  if (!fsReady) Serial.println("[FS] mount fail");

  // stato persistente
  prefs.begin("worktimer", false);
  loadConfig();
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
  if (ek == EV_SHORT) actionPrimary();
  if (ek == EV_LONG)  actionStop();
  if (eb == EV_SHORT) actionModeSwitch();
  if (eb == EV_LONG)  actionResetLog();

  // pomodoro: fine fase -> avanza
  if (mode == MODE_POMODORO && runSt == ST_RUNNING) {
    if (elapsedMs() >= pomoTargetMs) {
      // flash schermo come "beep" visivo
      tft.fillScreen(phaseColor());
      delay(120);
      tft.fillScreen(TFT_BLACK);
      pomoAdvance();
    }
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
