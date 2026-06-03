---
tags: [worktimer, moc]
---

# WorkTimer · Mappa

Knowledge base del firmware **GLR-WorkTimer** per LilyGo T-Display S3.
Firmware corrente: **v1.12.0**.

## Per l'utente
- [[Modalità]] — orologio, cronometro, pomodoro e i colori per modo
- [[Comandi]] — i due tasti fisici
- [[Dashboard web]] — controllo e configurazione da browser
- [[Configurazione]] — durate, fuso orario, luminosità, buzzer
- Guida passo-passo IT/EN: [[GUIDA]]

## Per lo sviluppatore
- [[Hardware]] — board, pin, alimentazione, batteria, buzzer
- [[Firmware e OTA]] — build, flash, aggiornamento via browser
- [[Architettura]] — com'è fatto `src/main.cpp`
- Riferimento sintetico: [[README]]

## In una riga
Quattro modalità su un LCD nero con cifre 7-segmenti grandi tinte per modo, due tasti,
stato persistente su NVS, e una dashboard web autonoma bilingue servita dal device.
