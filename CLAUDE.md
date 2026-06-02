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

3. **Upload needs MANUAL download mode; auto-reset is unreliable.** ESP32-S3 native USB CDC: once the TinyUSB app firmware runs, esptool's auto-reset can't enter the bootloader (`No serial data received` / `Could not open COM16`). To flash, have the user put the board in download mode: **hold BOOT, tap RST, release BOOT**, then flash with `--before no_reset`. `pio run -t upload` is flaky; the reliable path is direct esptool of just the app partition (skips rebuild + race):
   ```powershell
   $et="$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py"
   $b="C:\pio-ws\GLR-WorkTimer\build\lilygo-t-display-s3"
   python $et --chip esp32s3 --port COM16 --baud 921600 --before no_reset --after hard_reset write_flash -z 0x10000 "$b\firmware.bin"
   ```
   Full flash offsets: `0x0` bootloader, `0x8000` partitions, `0xe000` boot_app0, `0x10000` firmware.
   After flashing, software/esptool resets tend to land back in download mode (`boot:0x22`) because IO0 strapping is ambiguous over native USB — the user must press the physical **RST** for a normal app boot (`boot:0x2b`).

## Architecture

Everything is in `src/main.cpp` (~360 lines) driven by tunables in `include/config.h`. No classes beyond a `Button` struct; state is module-global.

**State model** — two orthogonal enums, both global:
- `Mode`: `MODE_CLOCK` / `MODE_STOPWATCH` / `MODE_POMODORO` (cycled by BOOT, only when idle).
- `RunSt`: `ST_IDLE` / `ST_RUNNING` / `ST_PAUSED` (driven by KEY).

**Elapsed-time accounting** is the core trick: `accumMs` holds closed segments, `segStart` marks the live segment; `elapsedMs()` = `accumMs + (now - segStart)` only while running. Pause folds the live segment into `accumMs`. This is what makes pause/resume and persistence work without a wall clock.

**Buttons** are polled (no interrupts) in `loop()` via `pollButton()`, which debounces and emits `EV_SHORT` / `EV_LONG`. Mapping: KEY short = start/pause/resume, KEY long = stop+save, BOOT short = switch mode, BOOT long (in clock) = wipe log. Holding **KEY at boot** forces the WiFi config portal (BOOT/GPIO0 is a strapping pin and is deliberately NOT used for this).

**Persistence (NVS via `Preferences`)**: `saveState()` is called on every state transition and every `STATE_SAVE_MS` while running. Because `millis()` resets on reboot, restored sessions come back as `ST_PAUSED` (elapsed preserved, user resumes with KEY) — never as running.

**Time/WiFi/web**: `setupTime()` uses WiFiManager's captive portal (AP `WorkTimer-Setup`); no hardcoded credentials. WiFi stays **on** after NTP sync (it used to power down) so `startWeb()` can serve a config page. `WebServer` on :80 + mDNS `worktimer.local` exposes a form to edit pomodoro durations; `handleSave()` clamps inputs, writes them to NVS via `saveConfig()`, and applies live only when the pomodoro is idle. Durations live in globals `cfgWork/cfgBreak/cfgLong/cfgCycles` (loaded by `loadConfig()`, defaults from `config.h` macros) — `pomoSetPhase()`/`pomoAdvance()` read these, not the macros. The board IP is shown bottom-right on the clock screen.

**Rendering**: full-screen 16-bit `TFT_eSprite` back-buffer (`render()` clears → draws → `pushSprite`), redrawn ~30 fps. Minimal black-on-white-corrected look: big white 7-segment digits (`drawBigTime()` picks Font8 ≤5 chars else Font7), dim grey labels (`DIM`). `tft.invertDisplay(true)` in `setup()` is mandatory or the panel shows inverted colors. Sprite needs PSRAM (`-DBOARD_HAS_PSRAM`); check serial for `[SPR] alloc fail`.

**Pomodoro** auto-advances in `loop()` when `elapsedMs() >= pomoTargetMs`: flashes the screen, logs completed work segments, sets the next phase via `pomoSetPhase()`, and restarts automatically. Long break every `POMO_CYCLES_TO_LONG` work segments.

**Session log**: appended to LittleFS `/sessions.csv` as `timestamp,kind,seconds` by `logSession()`.

## Display config

All TFT_eSPI pin/driver setup lives in `platformio.ini` `build_flags` (8-bit parallel, ST7789, 170x320). There is no `User_Setup.h` — do not add one; it would conflict. `PIN_POWER_ON` (GPIO15) must be driven HIGH in `setup()` or the LCD stays dark.
