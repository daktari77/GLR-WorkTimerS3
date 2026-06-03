---
tags: [worktimer, config]
---

# Configurazione

Due livelli: i **default** compilati in `include/config.h` e i **valori vivi** modificabili
dalla [[Dashboard web]] e persistiti su NVS (`Preferences`). I valori NVS vincono sui default.

## Dalla dashboard (consigliato)
Gruppo **Pomodoro**: Lavoro / Pausa / Pausa lunga (minuti), Cicli prima della pausa lunga.
Gruppo **Tempo**: Fuso orario e Ora legale da menu, con **anteprima dell'ora** che avrà il
device, così regoli finché coincide col tuo orologio (niente offset da indovinare).
Gruppo **Display**: Luminosità (cursore, immediato dopo Salva), Avanzamento automatico,
**Buzzer fine pomodoro** (richiede hardware, vedi [[Hardware#Buzzer]]; di serie off).

Le durate pomodoro si applicano subito solo a pomodoro fermo. Salva mostra **Salvato** in
verde, o errore in rosso.

## Default in `include/config.h`
| Macro | Significato |
|---|---|
| `FW_VERSION` | versione mostrata su INFO e dashboard |
| `POMO_WORK_MIN` / `POMO_BREAK_MIN` / `POMO_LONGBREAK_MIN` / `POMO_CYCLES_TO_LONG` | durate e cicli pomodoro |
| `GMT_OFFSET_SEC` / `DST_OFFSET_SEC` | fuso e ora legale di default |
| `SCREEN_BRIGHT` | luminosità backlight di default |
| `SLEEP_IDLE_MS` | timeout sleep schermo (default 2 min) |
| `BUZZER_PIN` / `BUZZER_DEFAULT` | pin buzzer (GPIO16) e stato iniziale |
| `STATE_SAVE_MS` | intervallo salvataggio stato mentre attivo |

Chiavi NVS lette da `loadConfig()` / scritte da `saveConfig()`: vedi [[Architettura#Persistenza]].
