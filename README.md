# GLR-WorkTimer

Work timer per **LilyGo T-Display S3** (ESP32-S3, LCD ST7789 320x170).

Guida utente passo-passo (IT/EN): **[GUIDA.md](GUIDA.md)**.

## Funzioni
- **Orologio** (idle): ora/data via NTP WiFi, cifre **ciano**, contatore sessioni salvate.
- **Cronometro**: cifre **ambra**, start / pausa / riprendi, salva durata a stop.
- **Pomodoro**: cifre **colore fase** (lavoro rosso / pausa verde / pausa lunga blu), cicli lavoro (25') / pausa (5') / pausa lunga (15' ogni 4), avanzamento automatico con flash schermo e **buzzer opzionale**.
- **Sleep schermo**: backlight off dopo 2 min in idle; qualunque tasto risveglia (`SLEEP_IDLE_MS`).
- **Log sessioni**: salvate su LittleFS in `/sessions.csv` (`timestamp,tipo,secondi`).
- **Stato persistente**: modo/fase/cicli/tempo salvati su NVS. Dopo reboot o calo corrente ripristina in **pausa** (riprendi con KEY).
- **Config WiFi captive** (WiFiManager): nessuna credenziale hardcoded.
- **Dashboard web** (`worktimer.local`): stato live + controllo remoto, config (durate pomodoro, fuso/ora legale a menu con anteprima ora, luminosità, avanzamento auto, buzzer), log con grafico 7 giorni, OTA, spegni WiFi, cancella log. Pagina unica autonoma, bilingue IT/EN.
- **Schermata INFO** (BOOT lungo): batteria %/tensione, ricarica, IP, versione firmware.
- **OTA** via browser: `/update`, nessun cavo.
- **Grafica**: sfondo nero, cifre 7-segmenti grandi (Font7) tinte per modo, etichette grigie discrete.

## Comandi (2 bottoni)
| Bottone | Tap | Pressione lunga (~0.7s) |
|---|---|---|
| **KEY** (basso, GPIO14) | start / pausa / riprendi | **stop + salva** sessione |
| **BOOT** (alto, GPIO0) | cambia modo (solo se fermo) | apre/chiude schermata **INFO** |

- **KEY all'accensione** → forza il portale di config WiFi (e riaccende la radio se spenta).
- Cancellare il log non è più un tasto: si fa dalla dashboard web (**Cancella log**).

## Setup
1. Installa **PlatformIO** (estensione VSCode) oppure CLI:
   ```
   pip install platformio
   ```
2. Fuso orario: impostalo dalla **dashboard web** (menu Fuso/Ora legale con anteprima dell'ora). I default sono in `include/config.h` (`GMT_OFFSET_SEC`, `DST_OFFSET_SEC`).

## Config WiFi (primo avvio)
- Al primo avvio (o se nessuna rete salvata) il timer crea un AP **`WorkTimer-Setup`** (pass `worktimer`).
- Connetti il telefono a quell'AP → si apre il portale captive → scegli la tua rete e inserisci la password.
- Per **riconfigurare**: tieni premuto **KEY** (bottone basso) durante l'accensione → riapre il portale.
- Credenziali e fuso sono salvati. Dopo il sync NTP la radio **resta accesa** per la dashboard web.

## Dashboard web
- Connesso al WiFi, l'IP è su **schermata INFO** (BOOT lungo) e nella dashboard. La mDNS è **`worktimer.local`**.
- Apri `http://worktimer.local` (o `http://<IP>`) dal browser sulla stessa rete.
- Stato live + controllo remoto (Avvia/Pausa/Stop/Modo), config pomodoro/fuso/luminosità/buzzer (**Salva** scrive su NVS), log + grafico, OTA.
- Dettaglio completo lato utente in **[GUIDA.md](GUIDA.md)**.

## Build & flash
```
pio run -j 1           # compila (sempre -j 1, vedi Note)
pio device monitor     # log seriale 115200
```
**Aggiornamento consigliato: OTA** (l'app già installata serve `/update`):
```
Invoke-WebRequest -Uri http://worktimer.local/update -Method Post `
  -Form @{ firmware = Get-Item ".pio/.../firmware.bin" }
```
**Primo flash via USB** (build senza OTA / recovery): l'auto-reset USB-JTAG è inaffidabile con l'app
in esecuzione. Metti la board in **download mode manuale** (tieni BOOT, premi RST, rilascia BOOT) e
flasha l'app a baud fisso:
```
python <pkg>/tool-esptoolpy/esptool.py --chip esp32s3 --port COM16 --baud 115200 \
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
`include/config.h`: durate pomodoro, pin, `FW_VERSION`, `SLEEP_IDLE_MS` (sleep schermo), `BUZZER_PIN` (GPIO16) / `BUZZER_DEFAULT` (buzzer fine pomodoro).
