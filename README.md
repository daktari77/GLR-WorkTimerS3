# GLR-WorkTimer

Work timer per **LilyGo T-Display S3** (ESP32-S3, LCD ST7789 320x170).

## Funzioni
- **Orologio** (idle): ora/data via NTP WiFi, contatore sessioni salvate.
- **Cronometro**: start / pausa / riprendi, salva durata a stop.
- **Pomodoro**: cicli lavoro (25') / pausa (5') / pausa lunga (15' ogni 4), avanzamento automatico con flash schermo.
- **Log sessioni**: salvate su LittleFS in `/sessions.csv` (`timestamp,tipo,secondi`).
- **Stato persistente**: modo/fase/cicli/tempo salvati su NVS. Dopo reboot o calo corrente ripristina in **pausa** (riprendi con KEY).
- **Config WiFi captive** (WiFiManager): nessuna credenziale hardcoded.
- **Web config pomodoro**: durate (lavoro/pausa/pausa lunga/cicli) modificabili da browser, salvate su NVS.
- **Grafica**: sfondo nero, cifre bianche grandi 7-segmenti (Font8), etichette grigie discrete.

## Comandi (2 bottoni)
| Bottone | Tap | Pressione lunga (~0.7s) |
|---|---|---|
| **KEY** (basso, GPIO14) | start / pausa / riprendi | **stop + salva** sessione |
| **BOOT** (alto, GPIO0) | cambia modo (solo se fermo) | reset log (solo in Orologio) |

## Setup
1. Installa **PlatformIO** (estensione VSCode) oppure CLI:
   ```
   pip install platformio
   ```
2. Regola fuso in `include/config.h`: `GMT_OFFSET_SEC=3600`, `DST_OFFSET_SEC=3600` (ora legale estiva) / `0` (inverno).

## Config WiFi (primo avvio)
- Al primo avvio (o se nessuna rete salvata) il timer crea un AP **`WorkTimer-Setup`** (pass `worktimer`).
- Connetti il telefono a quell'AP → si apre il portale captive → scegli la tua rete e inserisci la password.
- Per **riconfigurare**: tieni premuto **KEY** (bottone basso) durante l'accensione → riapre il portale.
- Credenziali e fuso sono salvati. Dopo il sync NTP la radio **resta accesa** per il web config.

## Web config pomodoro
- Connesso al WiFi, il display in modo Orologio mostra l'**IP** in basso a destra.
- Apri `http://<IP>` (o `http://worktimer.local`) dal browser sulla stessa rete.
- Form per lavoro / pausa / pausa lunga / cicli → **Salva** scrive su NVS e applica subito (se il pomodoro è fermo).

## Build & flash
```
pio run -j 1           # compila (sempre -j 1, vedi Note)
pio device monitor     # log seriale 115200
```
Upload: l'auto-reset USB-JTAG è inaffidabile con l'app in esecuzione. Metti la board in
**download mode manuale** (tieni BOOT, premi RST, rilascia BOOT) e flasha con esptool:
```
python <pkg>/tool-esptoolpy/esptool.py --chip esp32s3 --port COM16 --baud 921600 \
  --before no_reset --after hard_reset write_flash -z 0x10000 .pio/.../firmware.bin
```
Poi premi **RST** fisico per avviare l'app.

## Note
- Display T-Display S3 = interfaccia **parallela 8-bit** (non SPI). Build flags TFT_eSPI gia' in `platformio.ini`.
- `tft.invertDisplay(true)` necessario: senza, i colori del pannello sono invertiti.
- Sprite full-screen anti-flicker: richiede **PSRAM** (`-DBOARD_HAS_PSRAM`), presente sui T-Display S3 standard.
- `PIN_POWER_ON` (GPIO15) tenuto HIGH per alimentare LCD: necessario su questa board.
- Build su Google Drive corrompe i `.o`: `workspace_dir` punta a disco locale. Usa `-j 1` (race antivirus).

## Personalizzazione
Durate pomodoro e pin in `include/config.h`.
