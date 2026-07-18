# Installation Guide — ESP32-S3-LCD-2.8C

Instructions for setting up a fresh Windows machine from scratch to build and flash the project.

## Prerequisites

- Windows 10 or 11
- Internet connection
- ESP32-S3-LCD-2.8C board connected via **USB TO UART** port

## Step 1: Install Python

Open **PowerShell as Administrator**:

```powershell
winget install Python.Python.3.11
```

Close and reopen PowerShell after install.

## Step 2: Install Git

```powershell
winget install Git.Git
```

## Step 3: Install ESP-IDF 6.0.1

```powershell
# Clone ESP-IDF to C:\
cd C:\
git clone --recursive https://github.com/espressif/esp-idf.git -b v6.0.1

# Run the installer (downloads GCC, esptool, cmake, ninja, etc.)
cd C:\esp-idf
.\install.ps1 esp32s3
```

This downloads ~1 GB of toolchain binaries. Takes 5–10 minutes depending on connection speed.

## Step 4: Clone the project

```powershell
cd C:\
git clone https://github.com/Tinor2/DNT-MW-StudyBud.git
```

If you can't authenticate via HTTPS, copy the project folder from a USB drive to `C:\DNT-MW-StudyBud\` instead.

## Step 5: Build

```powershell
# Activate the ESP-IDF environment
C:\esp-idf\export.ps1

# Build the project
cd C:\DNT-MW-StudyBud
Remove-Item -Recurse -Force build
idf.py build
```

## Step 6: Flash and monitor

Plug the board into USB. Find the COM port in **Device Manager** (typically `COM3` or `COM4`).

```powershell
idf.py -p COM3 flash monitor
```

Replace `COM3` with the actual port number.

- The board should auto-reset and start flashing
- If it doesn't, hold **BOOT** button, press **RESET**, release **BOOT**
- Serial output appears after flashing completes
- Press `Ctrl+]` to exit the monitor

## Subsequent sessions

Only the export and flash steps are needed for later updates:

```powershell
C:\esp-idf\export.ps1
cd C:\DNT-MW-StudyBud
idf.py build
idf.py -p COM3 flash monitor
```

## Troubleshooting

### `install.ps1` blocked by execution policy

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope Process
```

Then retry the install command.

### Build fails with path-related errors

The `build/` directory may contain stale cache from a different machine. Clean it:

```powershell
Remove-Item -Recurse -Force build
idf.py build
```

### mbedtls / p256m errors

The esp-idf submodules may need updating:

```powershell
cd C:\esp-idf
git submodule update --init --recursive
```

### CH343P (Mac only) — bulk transfer corruption

The CH343P USB-UART bridge on this board may corrupt large data transfers on Mac. Windows does not have this issue. If you must flash from Mac, use:

```bash
esptool.py --port /dev/cu.usbserial-* --no-stub --baud 115200 \
  write_flash --flash_size keep \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/DNT-MW-StudyBud.bin
```
## Revised Plan

The recovery script failed because the manual low-level esptool API calls are incompatible with esptool v5.3.0 on ESP32-S3 (`erase_flash` ROM command not supported, flash command formatting changed). Here's a better approach:

### Step 1: Try the CLI-based esptool command (from your installation guide)

The installation guide already documents the correct workaround. Updated for esptool v5.3.0 syntax:

```bash
esptool --port /dev/cu.usbmodem59090523481 --no-stub --baud 115200 \
  write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/studybud.bin
```

Key flags: `--no-stub` (skips RAM stub upload), `--baud 115200` (avoids CH343P corruption).

If it hangs or fails to connect, **hold the BOOT button** on the board while running the command, then release after "Connecting..." appears.

### Step 2: If Step 1 fails — make `idf.py flash` work permanently

Add `CONFIG_ESPTOOLPY_NO_STUB=y` to `sdkconfig.defaults`, then rebuild:

```bash
idf.py fullclean && idf.py build && idf.py -p /dev/cu.usbmodem59090523481 flash
```

### Step 3: If all Mac flashing fails — follow the README

The README recommends using a **different computer** for the initial flash, then continuing development on your Mac.

### Step 4: Fix the recovery script

Update `flash_recovery.py` for esptool v5.3.0 API compatibility (fix `erase_flash`, use proper ESP32-S3 flash commands) so it works as a fallback.

---

**Start with Step 1** — it's the simplest and uses your existing documentation's recommended approach. Want me to proceed?

---

