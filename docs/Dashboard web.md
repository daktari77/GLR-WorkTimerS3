---
tags: [worktimer, web]
---

# Dashboard web

Pagina unica autonoma servita dal device su `:80` + mDNS **`worktimer.local`**.
Nessun asset esterno, niente CDN o framework; bilingue IT/EN gestito client-side
(scelta ricordata in `localStorage`). Architettura in [[Architettura#Web]].

Apri `http://worktimer.local` (o `http://<IP>`) dalla stessa rete WiFi.

## Stato
- Tempo grande: ora (orologio), trascorso (cronometro), rimanente (pomodoro).
- Modalità, fase pomodoro, stato (in corso / pausa / fermo), pallini dei cicli.
- **Controllo remoto**: Avvia/Pausa/Riprendi, Stop, Modo (come i tasti, da browser).
- Riga con `worktimer.local`, IP, **versione firmware**, batteria.
- Se il device non risponde compare un avviso **non raggiungibile**; se hai spento
  tu il WiFi lo dice esplicitamente.

## Configurazione
Campi raggruppati in **Pomodoro / Tempo / Display**, poi **Salva** (scrive su NVS).
Dettaglio in [[Configurazione]]. Il fuso si imposta da menu con **anteprima ora live**.

## Sessioni
Totali oggi/complessivo, pomodori di oggi, grafico 7 giorni, elenco recenti,
**CSV** (download `sessions.csv`), **Cancella log** (conferma a due tocchi).

## Sistema
- **Firmware**: pagina OTA, vedi [[Firmware e OTA]].
- **Spegni WiFi**: radio off per risparmio; pagina irraggiungibile finché non riaccendi
  tenendo **KEY** al boot ([[Comandi]]).

## Endpoint (per sviluppatori)
`GET /status`, `GET /log`, `POST /save`, `GET /cmd?a=…`, `POST /clearlog`,
`GET /sessions.csv`, `POST /wifioff`, `GET|POST /update`. Dettagli in [[Architettura#Web]].
