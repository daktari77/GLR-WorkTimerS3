# GLR-WorkTimer

Work timer per **LilyGo T-Display S3** (ESP32-S3, LCD ST7789 320x170).

## Funzioni
- **Orologio** (idle): ora/data via NTP WiFi, contatore sessioni salvate.
- **Cronometro**: start / pausa / riprendi, salva durata a stop.
- **Pomodoro**: cicli lavoro (25') / pausa (5') / pausa lunga (15' ogni 4), avanzamento automatico con flash schermo.
- **Log sessioni**: salvate su LittleFS in `/sessions.csv` (`timestamp,tipo,secondi`).
- **Stato persistente**: modo/fase/cicli/tempo salvati su NVS. Dopo reboot o calo corrente ripristina in **pausa** (riprendi con KEY).
- **Config WiFi captive** (WiFiManager): nessuna credenziale hardcoded.

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
- Credenziali e fuso sono salvati; la radio si spegne dopo il sync NTP (risparmio).

## Build & flash
```
pio run                # compila
pio run -t upload      # flash via USB-C
pio device monitor     # log seriale 115200
```

## Note
- Display T-Display S3 = interfaccia **parallela 8-bit** (non SPI). Build flags TFT_eSPI gia' in `platformio.ini`.
- Sprite full-screen anti-flicker: richiede **PSRAM** (`-DBOARD_HAS_PSRAM`), presente sui T-Display S3 standard.
- `PIN_POWER_ON` (GPIO15) tenuto HIGH per alimentare LCD: necessario su questa board.

## Personalizzazione
Durate pomodoro e pin in `include/config.h`.
