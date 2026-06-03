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

A connessione avvenuta il timer è raggiungibile su **`http://worktimer.local`**. L'indirizzo IP si legge nella schermata INFO (tieni premuto BOOT) e nella dashboard web.

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

Ogni modalità ha un colore delle cifre diverso, per riconoscerla a colpo d'occhio:

- **Orologio** (cifre **ciano**): mostra ora e data (sincronizzate via internet). È lo stato di riposo.
- **Cronometro** (cifre **ambra**): conta in avanti. KEY breve per avviare, di nuovo per pausa, KEY lunga per fermare e salvare.
- **Pomodoro** (cifre **colore della fase**: rosso lavoro, verde pausa, blu pausa lunga): alterna lavoro e pause. Parte dalla fase di lavoro, al termine passa alla pausa, e dopo un certo numero di cicli fa una pausa lunga. A ogni cambio fase lo schermo lampeggia e, se attivo, suona il buzzer.

**Pausa tra le fasi**: nelle impostazioni puoi scegliere se il pomodoro avanza da solo alla fase successiva, oppure se si ferma in pausa e aspetta che premi KEY per ripartire.

**Schermo a riposo**: dopo 2 minuti da fermo lo schermo si spegne per risparmiare. Premi un tasto qualsiasi per riaccenderlo (la prima pressione serve solo a svegliarlo, non avvia nulla).

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
- Riga con `worktimer.local`, l'IP, la versione del firmware e lo stato della batteria.
- Se la scheda non risponde, in alto compare un avviso (**dispositivo non raggiungibile**); se hai spento tu il WiFi, lo dice esplicitamente.

I campi sono raggruppati in **Pomodoro**, **Tempo** e **Display**. Premi **Salva** per applicare:
- **Pomodoro**: Lavoro / Pausa / Pausa lunga (minuti) e Cicli prima della pausa lunga.
- **Tempo**: **Fuso orario** e **Ora legale** scelti da menu; sotto compare l'**anteprima dell'ora** che avrà il dispositivo, così regoli finché coincide col tuo orologio (niente numeri da indovinare).
- **Display**: **Luminosità** (cursore, effetto immediato dopo Salva), **Avanzamento automatico** del pomodoro, **Buzzer fine pomodoro** (richiede un buzzer collegato; di serie è disattivato).

Le durate del pomodoro si applicano subito solo se il pomodoro è fermo, per non interrompere una sessione attiva. Il salvataggio mostra **Salvato** in verde, oppure un errore in rosso se non è andato a buon fine.

**Sessioni (in basso)**
- Totali di **oggi** e **complessivo**, e numero di **pomodori** completati oggi.
- Grafico a barre degli **ultimi 7 giorni**.
- Elenco delle sessioni recenti (orario, tipo, durata).
- **CSV**: scarica lo storico completo (`sessions.csv`).
- **Cancella log**: azzera lo storico. Premi una volta per armare (il tasto diventa rosso "Conferma?"), una seconda volta entro 3 secondi per confermare.

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

- **Non trovo `worktimer.local`**: usa l'IP mostrato nella schermata INFO (tieni premuto BOOT) o nella dashboard. Verifica di essere sulla stessa rete WiFi.
- **Lo schermo è nero**: probabilmente è in riposo (dopo 2 minuti da fermo). Premi un tasto per riaccenderlo.
- **L'ora è sbagliata**: regola Fuso e Ora legale dalla dashboard finché l'anteprima coincide col tuo orologio. D'estate Ora legale = estate, d'inverno = nessuna.
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

Once connected, the timer is reachable at **`http://worktimer.local`**. The IP address is shown on the INFO screen (hold BOOT) and in the web dashboard.

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

Each mode tints the digits a different color, so you recognize it at a glance:

- **Clock** (**cyan** digits): shows time and date (synced over the internet). The resting state.
- **Stopwatch** (**amber** digits): counts up. KEY short to start, again to pause, KEY long to stop and save.
- **Pomodoro** (**phase-colored** digits: red work, green break, blue long break): alternates work and breaks. It starts with a work phase, then a break, and after a set number of cycles a long break. The screen flashes on every phase change and, if enabled, the buzzer sounds.

**Pause between phases**: in settings you can choose whether the pomodoro advances to the next phase on its own, or stops in pause and waits for you to press KEY.

**Screen sleep**: after 2 minutes idle the screen turns off to save power. Press any button to wake it (the first press only wakes it, it does not start anything).

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
- A line with `worktimer.local`, the IP, the firmware version, and battery status.
- If the board stops responding, a banner appears at the top (**device unreachable**); if you turned WiFi off yourself, it says so explicitly.

Fields are grouped into **Pomodoro**, **Time**, and **Display**. Press **Save** to apply:
- **Pomodoro**: Work / Break / Long break (minutes) and Cycles before long break.
- **Time**: **Timezone** and **DST** picked from menus; below them a **live preview** of the time the device will show, so you adjust until it matches your watch (no numbers to guess).
- **Display**: **Brightness** (slider, takes effect right after Save), pomodoro **Auto-advance**, **Buzzer at pomodoro end** (needs a buzzer wired; off by default).

Pomodoro durations apply immediately only while the pomodoro is stopped, so an active session is not interrupted. Saving shows **Saved** in green, or an error in red if it failed.

**Sessions (bottom)**
- **Today** and **Total** times, and the number of **pomodoros** completed today.
- A bar chart of the **last 7 days**.
- A list of recent sessions (time, kind, duration).
- **CSV**: download the full history (`sessions.csv`).
- **Clear log**: wipes the history. Press once to arm (the button turns red "Confirm?"), press again within 3 seconds to confirm.

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

- **Can't find `worktimer.local`**: use the IP shown on the INFO screen (hold BOOT) or in the dashboard. Make sure you are on the same WiFi network.
- **Screen is black**: it is likely asleep (after 2 minutes idle). Press any button to wake it.
- **Wrong time**: adjust Timezone and DST in the dashboard until the live preview matches your watch. In summer set DST to summer, in winter to none.
- **Dark / dim display**: raise **Brightness** in settings.
- **Turned WiFi off and can't reach the page**: power on while holding **KEY** to reopen the portal, or reboot normally to reconnect to the saved network.
- **Session came back paused after a reboot**: this is intended. Elapsed time is kept; press **KEY** to resume.
- **Battery at 100% while on USB**: normal, the percentage is only reliable on battery.
