---
tags: [worktimer, uso]
---

# Modalità

Quattro modalità, ciclate da **BOOT** breve solo a timer fermo (vedi [[Comandi]]).
Ogni modalità tinge le cifre di un colore diverso per riconoscerla a colpo d'occhio.

| Modalità | Colore cifre | Cosa fa |
|---|---|---|
| **Orologio** | ciano | Ora e data via NTP. Stato di riposo. |
| **Cronometro** | ambra | Conta in avanti. Start/pausa/riprendi, salva a stop. |
| **Pomodoro** | colore fase | Lavoro/pausa/pausa lunga a cicli. |
| **Timer** | magenta | Countdown a durata libera, regolabile coi tasti. |

Colore guidato da `modeColor()`; per il pomodoro segue la fase: **rosso** lavoro,
**verde** pausa, **blu** pausa lunga.

## Orologio
Mostra ora e data sincronizzate. L'IP non è più su questo schermo: si legge nella
schermata INFO ([[Comandi#BOOT lungo INFO]]) e nella [[Dashboard web]]. In basso a
sinistra il conteggio sessioni salvate. Se l'ora non è sincronizzata le cifre restano grigie.

## Cronometro
Tempo crescente. La durata viene scritta nel log a ogni stop (vedi [[Architettura#Log sessioni]]).

## Pomodoro
Parte dal lavoro, poi pausa, e dopo `cicli` segmenti di lavoro una pausa lunga.
A ogni cambio fase lo schermo lampeggia e, se attivo, suona il buzzer ([[Hardware#Buzzer]]).
Durate e avanzamento si regolano dalla [[Configurazione]].

**Avanzamento**: automatico, oppure si ferma in pausa e attende **KEY** (impostabile).
Con avanzamento manuale, a fine fase lo schermo mostra un avviso lampeggiante
(**LAVORO FINITO** / **PAUSA FINITA**) e attende **KEY** per la fase successiva.

## Timer
Countdown a durata libera (default 10 min, 1–180). A timer fermo:
- **KEY** breve avvia il conto alla rovescia;
- **KEY** tenuto entra in **REGOLA DURATA** (vedi [[Comandi#Timer REGOLA]]);
- **BOOT** breve cambia modalità.

A countdown scaduto: flash (più buzzer se attivo), avviso **TEMPO SCADUTO · PREMI KEY**,
la sessione viene loggata come `timer` e si torna fermi. Nessun riavvio automatico.
La durata si imposta anche dalla [[Dashboard web]] (gruppo **Timer**).

## Schermo a riposo
Dopo 2 minuti da fermo il backlight si spegne (`SLEEP_IDLE_MS`). Un tasto qualsiasi
risveglia: la prima pressione serve solo a svegliare, non avvia nulla.
Mentre un timer gira o la schermata INFO è aperta, lo schermo non si spegne.
