// ============================================================
//  GLR-WorkTimer  -  LilyGo T-Display S3 (ESP32-S3)
//  Modi: CLOCK (idle) / STOPWATCH / POMODORO
//  KEY  (GPIO14): start / pausa / riprendi   | longpress: stop+salva
//  BOOT (GPIO0) : cambia modo (solo se fermo) | longpress in clock: reset log
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
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

// ---------------- Pomodoro logica ----------------
void pomoSetPhase(PomoPhase p) {
  pomoPhase = p;
  switch (p) {
    case PH_WORK:      pomoTargetMs = (uint32_t)POMO_WORK_MIN      * 60000UL; break;
    case PH_BREAK:     pomoTargetMs = (uint32_t)POMO_BREAK_MIN     * 60000UL; break;
    case PH_LONGBREAK: pomoTargetMs = (uint32_t)POMO_LONGBREAK_MIN * 60000UL; break;
  }
}

void resetSegment() { accumMs = 0; segStart = millis(); }

// avanza alla fase successiva del pomodoro (chiamato a fine countdown)
void pomoAdvance() {
  if (pomoPhase == PH_WORK) {
    logSession("pomodoro-work", pomoTargetMs);
    pomoDoneWork++;
    if (pomoDoneWork % POMO_CYCLES_TO_LONG == 0) pomoSetPhase(PH_LONGBREAK);
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

// card riusabile: bg slate, cifre trasparenti, seam opzionale, bordo colorato
void drawCard(int x, int y, int w, int h, const char* txt,
              uint8_t font, uint16_t border, bool seam) {
  uint16_t cardBg = tft.color565(28, 32, 44);   // slate scuro
  spr.fillRoundRect(x, y, w, h, 12, cardBg);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE);                  // trasparente: seam attraversa
  spr.drawString(txt, x + w/2, y + h/2, font);
  if (seam) spr.drawFastHLine(x + 3, y + h/2, w - 6, TFT_BLACK);  // linea di piega
  spr.drawRoundRect(x, y, w, h, 12, border);
}

void drawClock() {
  // layout card
  const int CY = 34, CH = 86, CW = 120;
  const int HX = 25, MX = 175;
  const int MIDX = HX + CW + (MX - (HX + CW)) / 2;   // centro tra le due card
  const int CMID = CY + CH / 2;

  char hh[4] = "--", mm[4] = "--";
  int sec = 0; bool synced = timeSynced;
  struct tm tmv;
  if (synced) {
    time_t t = time(nullptr); localtime_r(&t, &tmv);
    snprintf(hh, sizeof(hh), "%02d", tmv.tm_hour);
    snprintf(mm, sizeof(mm), "%02d", tmv.tm_min);
    sec = tmv.tm_sec;
  }

  // card ore / minuti
  uint16_t edge = tft.color565(64, 72, 92);
  drawCard(HX, CY, CW, CH, hh, 8, edge, true);
  drawCard(MX, CY, CW, CH, mm, 8, edge, true);

  // colon centrale lampeggiante (acceso su secondi pari)
  uint16_t dotCol = (synced && (sec % 2)) ? tft.color565(40,44,56) : TFT_CYAN;
  spr.fillRoundRect(MIDX - 4, CMID - 22, 9, 9, 3, dotCol);
  spr.fillRoundRect(MIDX - 4, CMID + 13, 9, 9, 3, dotCol);

  // barra secondi (progresso del minuto)
  const int BX = HX, BY = CY + CH + 12, BW = MX + CW - HX, BH = 5;
  spr.fillRoundRect(BX, BY, BW, BH, 2, tft.color565(40,44,56));
  if (synced) {
    int fw = (BW * sec) / 59;
    if (fw > 0) spr.fillRoundRect(BX, BY, fw, BH, 2, TFT_CYAN);
  }

  // data
  spr.setTextDatum(MC_DATUM);
  if (synced) {
    char buf[32];
    strftime(buf, sizeof(buf), "%a %d %b %Y", &tmv);
    spr.setTextColor(tft.color565(150,160,180), TFT_BLACK);
    spr.drawString(buf, SCR_W/2, BY + BH + 13, 2);
  } else {
    spr.setTextColor(TFT_ORANGE, TFT_BLACK);
    spr.drawString("no NTP - tieni KEY al boot per WiFi", SCR_W/2, BY + BH + 13, 2);
  }
}

void drawTimer() {
  char buf[32];
  bool pomo = (mode == MODE_POMODORO);
  uint16_t pcol = phaseColor();
  uint16_t scol = runSt == ST_RUNNING ? TFT_GREEN
                : runSt == ST_PAUSED  ? TFT_ORANGE
                : tft.color565(90, 96, 110);

  // chip fase (solo pomodoro): pill colorata
  if (pomo) {
    spr.fillRoundRect(90, 36, 140, 22, 8, pcol);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TFT_BLACK);
    spr.drawString(phaseName(pomoPhase), 160, 47, 2);
  }

  // tempo (countdown per pomodoro, crescente per cronometro) in card 7-seg
  if (pomo) {
    uint32_t e = elapsedMs();
    uint32_t rem = (e >= pomoTargetMs) ? 0 : (pomoTargetMs - e);
    fmtHMS(rem, buf, sizeof(buf));
  } else {
    fmtHMS(elapsedMs(), buf, sizeof(buf));
  }
  int cardY = pomo ? 64 : 52;
  int cardH = 60;
  drawCard(30, cardY, 260, cardH, buf, 7, scol, false);

  // barra progresso (solo pomodoro)
  int by = cardY + cardH + 9;
  if (pomo) {
    const int bw = 260;
    spr.fillRoundRect(30, by, bw, 6, 2, tft.color565(40, 44, 56));
    uint32_t e = elapsedMs();
    if (pomoTargetMs > 0) {
      uint32_t cap = e > pomoTargetMs ? pomoTargetMs : e;
      int fw = (int)((uint64_t)bw * cap / pomoTargetMs);
      if (fw > 0) spr.fillRoundRect(30, by, fw, 6, 2, pcol);
    }
    by += 12;
  }

  // stato
  const char* st = runSt == ST_RUNNING ? "RUNNING"
                 : runSt == ST_PAUSED  ? "PAUSA" : "PRONTO";
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(scol, TFT_BLACK);
  spr.drawString(st, SCR_W/2, by + 6, 4);

  // dot cicli pomodoro
  if (pomo) {
    int done = pomoDoneWork % POMO_CYCLES_TO_LONG;
    int n = POMO_CYCLES_TO_LONG;
    const int gap = 16;
    int sx = SCR_W/2 - (n - 1) * gap / 2;
    int dy = SCR_H - 8;
    for (int i = 0; i < n; i++) {
      uint16_t c = (i < done) ? pcol : tft.color565(60, 66, 82);
      spr.fillCircle(sx + i * gap, dy, 4, c);
    }
  }
}

void render() {
  spr.fillSprite(TFT_BLACK);

  // header: nome modo
  spr.setTextColor(phaseColor(), TFT_BLACK);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(modeName(mode), 6, 6, 4);

  // wifi/log indicatori in alto a destra
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(timeSynced ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
  spr.drawString(timeSynced ? "NTP" : "---", SCR_W-6, 8, 2);

  if (mode == MODE_CLOCK) {
    drawClock();
    // numero sessioni loggate
    char buf[24];
    snprintf(buf, sizeof(buf), "log: %d sess.", countSessions());
    spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
    spr.setTextDatum(BL_DATUM);
    spr.drawString(buf, 6, SCR_H-4, 2);
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
    Serial.printf("[WiFi] ok %s\n", WiFi.localIP().toString().c_str());
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
    struct tm tmv;
    if (getLocalTime(&tmv, 5000)) { timeSynced = true; Serial.println("[NTP] sync ok"); }
  } else {
    Serial.println("[WiFi] no connessione");
  }
  // spegni radio dopo sync per risparmio (ri-config via BOOT all'avvio)
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
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
  tft.fillScreen(TFT_BLACK);
  spr.setColorDepth(16);
  if (!spr.createSprite(SCR_W, SCR_H)) {
    Serial.println("[SPR] alloc fail");  // serve PSRAM
  }

  fsReady = LittleFS.begin(true);
  if (!fsReady) Serial.println("[FS] mount fail");

  // stato persistente
  prefs.begin("worktimer", false);
  loadState();
  pomoSetPhase(pomoPhase);   // imposta pomoTargetMs per la fase ripristinata

  // KEY tenuto all'accensione -> forza portale config WiFi
  // (non uso BOOT/GPIO0: e' strapping pin, tenuto al reset entra in download mode)
  bool forcePortal = (digitalRead(PIN_BTN_KEY) == LOW);
  setupTime(forcePortal);

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

  // salvataggio periodico mentre attivo (recupero dopo calo corrente)
  if (runSt == ST_RUNNING && (millis() - lastStateSave) > STATE_SAVE_MS) {
    saveState();
  }

  render();
  delay(33);   // ~30 fps
}
