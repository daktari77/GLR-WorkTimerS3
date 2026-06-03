---
tags: [worktimer, uso]
---

# Comandi

Due soli tasti. **KEY** (basso, GPIO14) = azione; **BOOT** (alto, GPIO0) = navigazione.
Polling con debounce, niente interrupt (vedi [[Architettura#Bottoni]]).

| Tasto | Pressione | Effetto |
|---|---|---|
| KEY | breve | Avvia / pausa / riprendi |
| KEY | lunga (~0.7s) | Ferma e salva la sessione |
| BOOT | breve | Cambia [[Modalità]] (solo a timer fermo) |
| BOOT | lunga | Apre/chiude la schermata **INFO** |
| KEY | all'accensione | Forza il portale WiFi (e riaccende la radio se spenta) |

La modalità cambia solo a timer fermo, così non si interrompe una sessione attiva.

## BOOT lungo: INFO
Mostra **batteria** (% e tensione, o "USB"), **ricarica** SI/NO, **IP**, **versione firmware**.
Si chiude con una pressione breve o da sola dopo 8 secondi.

## Cancellare il log
Non è più un tasto fisico: si fa dalla [[Dashboard web]] con **Cancella log**
(conferma a due tocchi).

> BOOT/GPIO0 è uno strapping pin: tenuto al reset entra in download mode, perciò il
> portale WiFi si forza con **KEY** al boot, non con BOOT. Vedi [[Firmware e OTA#Flash USB]].
