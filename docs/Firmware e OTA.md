---
tags: [worktimer, build, ota]
---

# Firmware e OTA

PlatformIO + Arduino. Nessun test, nessun lint: firmware nudo. Versione in `FW_VERSION`
([[Configurazione]]); aggiornala a ogni rilascio.

## Build
```
pio run -j 1            # sempre -j 1
pio device monitor      # seriale 115200
```
- **`-j 1` obbligatorio**: Windows Defender blocca i `.o` freschi, il build parallelo va in
  race (`ar: ... No such file`).
- Il progetto sta su Google Drive (`G:`) ma il build no: `workspace_dir` punta a disco locale
  (`C:/pio-ws/...`), altrimenti la sync corrompe i `.o`. Non rimuovere quella riga.

## OTA (consigliato)
L'app in esecuzione serve `/update` (basato su `Update.h`). Senza cavo né tasti:
```
Invoke-WebRequest -Uri http://worktimer.local/update -Method Post `
  -Form @{ firmware = Get-Item "C:\pio-ws\GLR-WorkTimer\build\lilygo-t-display-s3\firmware.bin" }
```
Risponde `{"ok":true}`, si riavvia, torna online in ~2s. Device `192.168.1.200` / `worktimer.local`.
Pagina utente: [[Dashboard web#Sistema]].

## Flash USB
Solo per il primo flash di una build senza OTA o per recovery. L'auto-reset USB-JTAG è
inaffidabile con l'app attiva. Metti la board in **download mode** (tieni BOOT, premi RST,
rilascia BOOT → LCD nero) e flasha a baud fisso senza rinegoziazione:
```
python <pkg>/tool-esptoolpy/esptool.py --chip esp32s3 --port COM16 --baud 115200 \
  --before no_reset --after hard_reset write_flash -z 0x10000 <build>/firmware.bin
```
Offsets full flash: `0x0` bootloader, `0x8000` partizioni, `0xe000` boot_app0, `0x10000` app.
Poi premi **RST** per avviare. Se la board non enumera più, scollega/ricollega l'USB-C
(sospetta cavo solo-carica).
