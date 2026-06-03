# GLR-WorkTimer

**[Italiano](#italiano) · [English](#english)**

---

# Italiano

Timer da scrivania su LilyGo T-Display S3. Tre modalità (orologio, cronometro, pomodoro), due tasti fisici e una dashboard web raggiungibile da telefono o computer sulla stessa rete WiFi.

## 1. Primo avvio: configurare il WiFi

Alla prima accensione (o se non trova una rete salvata) il timer crea un punto di accesso WiFi per la configurazione.

1. Sul telefono apri le impostazioni WiFi e collegati alla rete **`WorkTimer-Setup`** (password `worktimer`).
2. Si apre da sola la pagina del portale. Se non si apre, vai su `http://192.168.4.1`.
3. Scegli la tua rete di casa, inserisci la password, salva.
4. Il timer si riavvia e si collega. Il portale scade dopo 120 secondi.

Per riaprire il portale in seguito: **tieni premuto il tasto KEY mentre accendi** la scheda.

A connessione avvenuta il display mostra in basso a destra l'indirizzo IP, e il timer è raggiungibile su **`http://worktimer.local`**.

## 2. I due tasti fisici

La scheda ha due tasti: **KEY** (in basso, azione) e **BOOT** (in alto, navigazione).

| Tasto | Pressione | Effetto |
|-------|-----------|---------|
| KEY  | breve | Avvia / metti in pausa / riprendi |
| KEY  | lunga | Ferma e salva la sessione |
| BOOT | breve | Cambia modalità (solo a timer fermo) |
| BOOT | lunga | Apre la schermata INFO (batteria, ricarica, IP) |
| KEY  | all'accensione | Forza il portale di configurazione WiFi |

La modalità si cambia solo quando il timer è fermo, così non si interrompe una sessione in corso.

## 3. Le tre modalità

- **Orologio**: mostra ora e data (sincronizzate via internet). È lo stato di riposo.
- **Cronometro**: conta in avanti. KEY breve per avviare, di nuovo per pausa, KEY lunga per fermare e salvare.
- **Pomodoro**: alterna lavoro e pause. Parte dalla fase di lavoro, al termine passa alla pausa, e dopo un certo numero di cicli fa una pausa lunga. A ogni cambio fase lo schermo lampeggia.

**Pausa tra le fasi**: nelle impostazioni puoi scegliere se il pomodoro avanza da solo alla fase successiva, oppure se si ferma in pausa e aspetta che premi KEY per ripartire.

## 4. Schermata INFO (batteria)

Tieni premuto **BOOT** per aprire la schermata INFO. Mostra:

- **Batteria**: percentuale e tensione (oppure "USB" se non c'è una batteria collegata).
- **Ricarica**: SI / NO.
- **IP**: indirizzo della scheda sulla rete.

Si chiude con una pressione breve di un tasto, oppure da sola dopo 8 secondi.

> Nota: con la scheda alimentata via USB la batteria risulta carica al 100% / in carica. La percentuale è significativa solo a batteria.

## 5. Dashboard web

Apri **`http://worktimer.local`** dal telefono o dal computer. Niente app, niente account. In alto a destra il pulsante **EN/IT** cambia la lingua (la scelta viene ricordata).

**Stato (in alto)**
- Numero grande con il tempo in corso: l'ora in modalità orologio, il tempo trascorso col cronometro, il tempo rimanente nel pomodoro.
- Etichetta della modalità, fase del pomodoro e stato (in corso / in pausa / fermo).
- Pallini dei cicli del pomodoro.
- Pulsanti di **controllo remoto**: Avvia/Pausa/Riprendi, Stop, Modo. Fanno quello che farebbero i tasti fisici, ma dal browser.
- Riga con `worktimer.local`, l'IP e lo stato della batteria.

**Configurazione (al centro)**, poi premi **Salva**:
- **Lavoro / Pausa / Pausa lunga** (minuti) e **Cicli prima della pausa lunga**.
- **Fuso orario** e **Ora legale** (in ore). In Italia: fuso `1`, ora legale `1` d'estate e `0` d'inverno.
- **Luminosità** dello schermo (cursore, effetto immediato dopo Salva).
- **Avanzamento automatico** del pomodoro.

Le durate del pomodoro si applicano subito solo se il pomodoro è fermo, per non interrompere una sessione attiva.

**Sessioni (in basso)**
- Totali di **oggi** e **complessivo**, e numero di **pomodori** completati oggi.
- Grafico a barre degli **ultimi 7 giorni**.
- Elenco delle sessioni recenti (orario, tipo, durata).
- **CSV**: scarica lo storico completo (`sessions.csv`).
- **Cancella log**: azzera lo storico (chiede conferma).

**Sistema**
- **Firmware**: apre la pagina di aggiornamento (vedi sotto).
- **Spegni WiFi**: spegne la radio per risparmiare batteria. Attenzione: dopo lo spegnimento la pagina non è più raggiungibile finché non riaccendi tenendo premuto **KEY all'avvio**.

**Notifiche**: al cambio di fase del pomodoro la dashboard mostra un avviso a schermo. Se autorizzi le notifiche del browser (te lo chiede al primo tocco di un pulsante), ricevi anche una notifica di sistema.

## 6. Aggiornare il firmware (OTA, senza cavo)

1. Apri **`http://worktimer.local/update`**.
2. Scegli il file `firmware.bin`.
3. Premi **Carica**. Una barra mostra l'avanzamento.
4. Al termine la scheda si riavvia da sola e torna online in pochi secondi.

Nessun cavo, nessuna combinazione di tasti.

## 7. Problemi comuni

- **Non trovo `worktimer.local`**: usa l'IP mostrato sul display (orologio in basso a destra, o schermata INFO). Verifica di essere sulla stessa rete WiFi.
- **L'ora è sbagliata**: controlla Fuso e Ora legale. D'estate Ora legale = `1`, d'inverno = `0`.
- **Display scuro / poco luminoso**: alza la **Luminosità** nelle impostazioni.
- **Ho spento il WiFi e non raggiungo più la pagina**: riaccendi la scheda tenendo premuto **KEY** per riaprire il portale, oppure riavvia normalmente per riconnetterti alla rete salvata.
- **La sessione è ripartita in pausa dopo un riavvio**: è voluto. Il tempo è conservato; premi **KEY** per riprendere.
- **Batteria al 100% mentre è collegato l'USB**: normale, la percentuale è attendibile solo a batteria.

---

# English

A desk timer on the LilyGo T-Display S3. Three modes (clock, stopwatch, pomodoro), two physical buttons, and a web dashboard reachable from a phone or computer on the same WiFi network.

## 1. First boot: WiFi setup

On first power-up (or when no saved network is found) the timer creates a WiFi access point for configuration.

1. On your phone open WiFi settings and join the network **`WorkTimer-Setup`** (password `worktimer`).
2. The portal page opens by itself. If it does not, go to `http://192.168.4.1`.
3. Pick your home network, enter the password, save.
4. The timer reboots and connects. The portal times out after 120 seconds.

To reopen the portal later: **hold KEY while powering on** the board.

Once connected, the display shows the IP address at the bottom right, and the timer is reachable at **`http://worktimer.local`**.

## 2. The two physical buttons

The board has two buttons: **KEY** (bottom, action) and **BOOT** (top, navigation).

| Button | Press | Effect |
|--------|-------|--------|
| KEY  | short | Start / pause / resume |
| KEY  | long  | Stop and save the session |
| BOOT | short | Switch mode (only while stopped) |
| BOOT | long  | Open the INFO screen (battery, charging, IP) |
| KEY  | at power-on | Force the WiFi config portal |

Mode only changes while the timer is stopped, so a running session is never interrupted.

## 3. The three modes

- **Clock**: shows time and date (synced over the internet). The resting state.
- **Stopwatch**: counts up. KEY short to start, again to pause, KEY long to stop and save.
- **Pomodoro**: alternates work and breaks. It starts with a work phase, then a break, and after a set number of cycles a long break. The screen flashes on every phase change.

**Pause between phases**: in settings you can choose whether the pomodoro advances to the next phase on its own, or stops in pause and waits for you to press KEY.

## 4. INFO screen (battery)

Hold **BOOT** to open the INFO screen. It shows:

- **Battery**: percentage and voltage (or "USB" when no battery is connected).
- **Charging**: SI / NO (yes / no).
- **IP**: the board's address on the network.

It closes on a short button press, or by itself after 8 seconds.

> Note: when the board is powered over USB the battery reads full / charging. The percentage is only meaningful on battery.

## 5. Web dashboard

Open **`http://worktimer.local`** from a phone or computer. No app, no account. The **EN/IT** button at the top right switches language (your choice is remembered).

**Status (top)**
- A large number with the current time: the clock in clock mode, elapsed time on the stopwatch, remaining time in the pomodoro.
- Mode label, pomodoro phase, and state (running / paused / idle).
- Pomodoro cycle dots.
- **Remote control** buttons: Start/Pause/Resume, Stop, Mode. They do what the physical buttons do, from the browser.
- A line with `worktimer.local`, the IP, and battery status.

**Configuration (middle)**, then press **Save**:
- **Work / Break / Long break** (minutes) and **Cycles before long break**.
- **Timezone** and **DST** (in hours). For Italy: timezone `1`, DST `1` in summer and `0` in winter.
- Screen **Brightness** (slider, takes effect right after Save).
- Pomodoro **Auto-advance**.

Pomodoro durations apply immediately only while the pomodoro is stopped, so an active session is not interrupted.

**Sessions (bottom)**
- **Today** and **Total** times, and the number of **pomodoros** completed today.
- A bar chart of the **last 7 days**.
- A list of recent sessions (time, kind, duration).
- **CSV**: download the full history (`sessions.csv`).
- **Clear log**: wipes the history (asks for confirmation).

**System**
- **Firmware**: opens the update page (see below).
- **WiFi off**: turns the radio off to save battery. Warning: after turning it off the page is no longer reachable until you power on while holding **KEY**.

**Notifications**: on a pomodoro phase change the dashboard shows an on-screen banner. If you allow browser notifications (it asks on your first button tap), you also get a system notification.

## 6. Updating the firmware (OTA, no cable)

1. Open **`http://worktimer.local/update`**.
2. Choose the `firmware.bin` file.
3. Press **Upload**. A bar shows progress.
4. When done the board reboots on its own and is back online in a few seconds.

No cable, no button combination.

## 7. Troubleshooting

- **Can't find `worktimer.local`**: use the IP shown on the display (clock bottom right, or the INFO screen). Make sure you are on the same WiFi network.
- **Wrong time**: check Timezone and DST. In summer DST = `1`, in winter = `0`.
- **Dark / dim display**: raise **Brightness** in settings.
- **Turned WiFi off and can't reach the page**: power on while holding **KEY** to reopen the portal, or reboot normally to reconnect to the saved network.
- **Session came back paused after a reboot**: this is intended. Elapsed time is kept; press **KEY** to resume.
- **Battery at 100% while on USB**: normal, the percentage is only reliable on battery.
