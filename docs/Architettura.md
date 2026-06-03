---
tags: [worktimer, dev]
---

# Architettura

Tutto in `src/main.cpp` (~circa 360+ righe), tunable in `include/config.h`.
Nessuna classe oltre allo struct `Button`; lo stato è globale di modulo.

## Stato
Due enum ortogonali, entrambi globali:
- `Mode`: `MODE_CLOCK` / `MODE_STOPWATCH` / `MODE_POMODORO` / `MODE_TIMER` (ciclato da BOOT, solo a fermo).
- `RunSt`: `ST_IDLE` / `ST_RUNNING` / `ST_PAUSED` (guidato da KEY).

## Conteggio tempo
`accumMs` somma i segmenti chiusi, `segStart` marca il segmento vivo;
`elapsedMs() = accumMs + (now - segStart)` solo mentre in corso. La pausa ripiega il
segmento vivo in `accumMs`. Niente orologio a muro: regge pausa/riprendi e persistenza.

## Bottoni
Polling in `loop()` via `pollButton()` (debounce, emette `EV_SHORT`/`EV_LONG`).
Mapping in [[Comandi]]. La gestione sleep schermo intercetta il primo tasto per risvegliare
senza eseguire azioni.

## Persistenza
NVS via `Preferences`. `saveState()` a ogni transizione e ogni `STATE_SAVE_MS` mentre attivo.
Poiché `millis()` riparte al reboot, le sessioni ripristinate tornano in `ST_PAUSED`
(tempo conservato, l'utente riprende con KEY), mai in corso.
Config: `loadConfig()`/`saveConfig()` (chiavi `cwork/cbreak/clong/ccyc/ctmr/cgmt/cdst/cauto/cbright/cbuzz`).

## Rendering
Back-buffer `TFT_eSprite` 16-bit full-screen (`render()` pulisce → disegna → `pushSprite`),
~30 fps. Cifre 7-segmenti (Font7) tinte da `modeColor()` (vedi [[Modalità]]); etichette grigie.
Richiede PSRAM. `tft.invertDisplay(true)` obbligatorio ([[Hardware]]).

## Web
`WebServer` :80 + mDNS, pagina unica PROGMEM `PAGE_HTML` via `send_P`. Il client fa polling
di `/status` (~1s) e `/log` (~15s) e aggiorna il DOM. Endpoint elencati in
[[Dashboard web#Endpoint per sviluppatori]]. Time/WiFi via WiFiManager (captive, nessuna
credenziale hardcoded); la radio resta accesa dopo il sync NTP per servire la dashboard.

## Log sessioni
Append su LittleFS `/sessions.csv` come `timestamp,kind,seconds` da `logSession()`.

> Vedi anche le note di build in [[Firmware e OTA]] e il riferimento sintetico [[README]].
