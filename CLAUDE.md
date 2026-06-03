# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a work timer running on a **LilyGo T-Display S3** (ESP32-S3, ST7789 320x170 parallel 8-bit LCD). PlatformIO + Arduino framework. Single-board embedded project, no host-side code.

## Build / upload (Windows, this machine)

`pio` is **not on PATH**. Prefix every command:
```powershell
$env:Path += ";C:\Users\Utente\AppData\Local\Python\pythoncore-3.14-64\Scripts"
```

```powershell
pio run -j 1                 # build (always use -j 1, see below)
pio device list              # find board (COM16, VID 303A:1001)
pio device monitor           # serial @ 115200
```

There is no test suite and nothing to lint — this is bare firmware.

### Three build/upload gotchas that WILL bite (all confirmed on hardware)

1. **Project lives on Google Drive (`G:\`); build must not.** Drive sync corrupts `.o` files mid-archive → `ar: ...cpp.o: No such file or directory`. `platformio.ini` sets `workspace_dir = C:/pio-ws/GLR-WorkTimer` to keep all of `.pio` on local disk. Do not remove it.

2. **Always build with `-j 1`.** Windows Defender locks freshly-written `.o` files; parallel build races → same `ar: No such file` error. Editing `platformio.ini` invalidates everything and forces a full rebuild where this resurfaces — just rerun, scons resumes incrementally.

3. **Prefer OTA over WiFi; USB only for the first OTA-less flash.** The firmware serves `/update` (an `Update.h` upload page). Reflash with no cable, no buttons:
   ```powershell
   Invoke-WebRequest -Uri http://worktimer.local/update -Method Post `
     -Form @{ firmware = Get-Item "C:\pio-ws\GLR-WorkTimer\build\lilygo-t-display-s3\firmware.bin" }
   ```
   Returns `{"ok":true}`, auto-reboots, back online in ~2s. Device IP `192.168.1.200`, mDNS `worktimer.local`. Use this whenever the running firmware already has OTA.

   **USB path (first flash of an OTA-less build, or recovery): auto-reset is unreliable.** `pio run -t upload` and any esptool run that renegotiates to 921600 die at `No serial data received` after the stub loads; native USB CDC can't re-enter the bootloader with an app running. Reliable: put the board in download mode (**hold BOOT, tap RST, release BOOT** — LCD goes dark), then flash just the app at **`--baud 115200 --before no_reset`** (no baud renegotiation):
   ```powershell
   $et="$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py"
   $b="C:\pio-ws\GLR-WorkTimer\build\lilygo-t-display-s3"
   python $et --chip esp32s3 --port COM16 --baud 115200 --before no_reset --after hard_reset write_flash -z 0x10000 "$b\firmware.bin"
   ```
   Full flash offsets: `0x0` bootloader, `0x8000` partitions, `0xe000` boot_app0, `0x10000` firmware. If the board stops enumerating entirely (no `303A` COM in `pio device list`), unplug/replug the USB-C cable; suspect a charge-only cable. After flashing, press the physical **RST** for a normal app boot.

## Architecture

Everything is in `src/main.cpp` (~360 lines) driven by tunables in `include/config.h`. No classes beyond a `Button` struct; state is module-global.

**State model** — two orthogonal enums, both global:
- `Mode`: `MODE_CLOCK` / `MODE_STOPWATCH` / `MODE_POMODORO` (cycled by BOOT, only when idle).
- `RunSt`: `ST_IDLE` / `ST_RUNNING` / `ST_PAUSED` (driven by KEY).

**Elapsed-time accounting** is the core trick: `accumMs` holds closed segments, `segStart` marks the live segment; `elapsedMs()` = `accumMs + (now - segStart)` only while running. Pause folds the live segment into `accumMs`. This is what makes pause/resume and persistence work without a wall clock.

**Buttons** are polled (no interrupts) in `loop()` via `pollButton()`, which debounces and emits `EV_SHORT` / `EV_LONG`. Mapping: KEY short = start/pause/resume, KEY long = stop+save, BOOT short = switch mode, BOOT long = toggle the INFO screen (battery %/voltage, charging Y/N, IP; short-press or 8s auto-hides). Log wipe is no longer a button; it's the web **Cancella log** action (`/clearlog`). Holding **KEY at boot** forces the WiFi config portal (BOOT/GPIO0 is a strapping pin and is deliberately NOT used for this).

**Persistence (NVS via `Preferences`)**: `saveState()` is called on every state transition and every `STATE_SAVE_MS` while running. Because `millis()` resets on reboot, restored sessions come back as `ST_PAUSED` (elapsed preserved, user resumes with KEY) — never as running.

**Time/WiFi/web**: `setupTime()` uses WiFiManager's captive portal (AP `WorkTimer-Setup`); no hardcoded credentials. WiFi stays **on** after NTP sync so `startWeb()` can serve the dashboard. NTP offsets are configurable (`cfgGmtMin`/`cfgDstMin`, applied via `configTime()`), not the `config.h` macros directly.

`WebServer` on :80 + mDNS `worktimer.local` serves a **single self-contained dashboard** (one PROGMEM page `PAGE_HTML`, sent with `send_P`; no CDN, framework, or external font — bilingual IT/EN handled client-side via `localStorage`). The client polls `/status` (~1s) and `/log` (~15s) and patches the DOM in place. Endpoints:
- `GET /status` → JSON: `mode/state/phase/elapsed/target/done/cycles`, config echo (`cwork/cbreak/clong/cgmt/cdst/cauto/cbright`), `batmv`+`charging`, `synced`, `ip`.
- `GET /log` → JSON: last 50 rows newest-first, `today`/`total` totals, `pomos` (today's pomodoro-work count), `days[7]` (7-day second buckets for the bar chart). Reads all of `/sessions.csv` into a `std::vector<String>`.
- `POST /save` → `handleSave()` clamps inputs to NVS via `saveConfig()`, applies brightness + timezone live, pomodoro durations live only when idle. Returns `{"ok":true}`.
- `GET /cmd?a=start|pause|resume|stop|mode` → remote buttons, calls the same `action*()` funcs as the physical keys.
- `POST /clearlog`, `GET /sessions.csv` (download), `POST /wifioff` (radio off; re-enable by holding KEY at boot), `GET|POST /update` (browser OTA via `Update.h`).

Config globals `cfgWork/cfgBreak/cfgLong/cfgCycles` + `cfgGmtMin/cfgDstMin/cfgAutoAdv/cfgBright` are loaded by `loadConfig()` (defaults from `config.h`) — `pomoSetPhase()`/`pomoAdvance()` read these, not the macros. Backlight is on LEDC PWM (`applyBright()` writes `cfgBright`). Battery is read on `PIN_BAT_ADC` (GPIO4) via `batteryMv()` (`analogReadMilliVolts × 2`, 8-sample average). The board IP shows bottom-right on the clock screen; battery/charge/IP also have a dedicated INFO screen on BOOT-long.

**Rendering**: full-screen 16-bit `TFT_eSprite` back-buffer (`render()` clears → draws → `pushSprite`), redrawn ~30 fps. Minimal black-on-white-corrected look: big white 7-segment digits (`drawBigTime()` picks Font8 ≤5 chars else Font7), dim grey labels (`DIM`). `tft.invertDisplay(true)` in `setup()` is mandatory or the panel shows inverted colors. Sprite needs PSRAM (`-DBOARD_HAS_PSRAM`); check serial for `[SPR] alloc fail`.

**Pomodoro** advances in `loop()` when `elapsedMs() >= pomoTargetMs`: flashes the screen, then `pomoAdvance()` logs the completed work segment and sets the next phase via `pomoNextPhase()`. If `cfgAutoAdv` it restarts running; otherwise it lands in `ST_PAUSED` awaiting KEY. Long break every `cfgCycles` work segments.

**Session log**: appended to LittleFS `/sessions.csv` as `timestamp,kind,seconds` by `logSession()`.

## Display config

All TFT_eSPI pin/driver setup lives in `platformio.ini` `build_flags` (8-bit parallel, ST7789, 170x320). There is no `User_Setup.h` — do not add one; it would conflict. `PIN_POWER_ON` (GPIO15) must be driven HIGH in `setup()` or the LCD stays dark.
