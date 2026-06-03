---
tags: [worktimer, hardware]
---

# Hardware

**LilyGo T-Display S3**: ESP32-S3, LCD ST7789 320x170 a interfaccia **parallela 8-bit**
(non SPI), PSRAM. Setup pin/driver TFT_eSPI nei `build_flags` di `platformio.ini`
(niente `User_Setup.h`).

## Pin
| Pin | Uso |
|---|---|
| GPIO15 | `PIN_POWER_ON` — deve stare HIGH o l'LCD resta spento |
| GPIO14 | KEY (azione) — vedi [[Comandi]] |
| GPIO0 | BOOT (navigazione, strapping pin) |
| GPIO4 | ADC partitore batteria (Vbatt = lettura × 2) |
| GPIO16 | Buzzer passivo (default, configurabile) |

## Alimentazione
`PIN_POWER_ON` HIGH in `setup()`. Backlight su PWM LEDC (canale 0); la luminosità
applicata da `applyBright()` segue il valore in [[Configurazione]].

## Batteria
Letta su GPIO4 (`analogReadMilliVolts × 2`, media di 8 campioni) con **debounce**: il valore
è campionato al massimo una volta ogni 5s e messo in cache. Percentuale stimata fra
`BAT_MV_EMPTY` e `BAT_MV_FULL`. Su USB risulta carica al 100%; la % è attendibile solo a batteria.
Visibile sulla schermata INFO ([[Comandi#BOOT lungo INFO]]) e nella [[Dashboard web]].

## Buzzer
Buzzer **passivo** su GPIO16, canale LEDC 1 (`ledcWriteTone`). Suona a fine fase pomodoro:
tono in salita a fine lavoro, in discesa a fine pausa. Disattivato di serie; si abilita dalla
[[Configurazione]] una volta cablato (buzzer fra GPIO16 e GND). Senza hardware il toggle è innocuo.

> Nota colori: `tft.invertDisplay(true)` è obbligatorio, altrimenti il pannello mostra colori invertiti.
