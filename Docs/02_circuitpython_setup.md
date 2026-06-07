# CircuitPython Setup & Workflow

## What is CircuitPython?

CircuitPython is a beginner-friendly derivative of MicroPython designed for microcontrollers. It requires no desktop IDE to get started — you edit files directly on a USB drive that appears on your computer. Code runs automatically on save.

---

## Installing CircuitPython

The Qualia S3 requires the **latest unstable (development) release** of CircuitPython — not the stable release.

1. Visit [circuitpython.org](https://circuitpython.org) and find your board
2. Under **Absolute Newest**, download the latest `.uf2` file
3. Double-tap the Reset button on the board — the board enters bootloader mode
   - For this board: tap reset, wait ~half a second, tap reset again (no NeoPixel to guide you)
4. A drive called `TFT_S3BOOT` appears on your computer
5. Drag the `.uf2` file onto `TFT_S3BOOT`
6. The drive disappears and `CIRCUITPY` appears — you're done

> **Common issue:** Charge-only USB cables will not work. Use a known data-sync cable.

> **If double-tap doesn't work:** The board may not have a UF2 bootloader installed. Refer to the "Install UF2 Bootloader" section of the full guide.

---

## The CIRCUITPY Drive

Once CircuitPython is installed, the board appears as a USB drive called `CIRCUITPY`. This is your working environment:

- **`code.py`** — your main program, runs automatically on boot or save
- **`lib/`** — folder for CircuitPython libraries
- **`boot.py`** — optional, runs before `code.py`, can configure USB behaviour

Any change to files on `CIRCUITPY` (save, rename, delete) triggers an automatic board reset and code re-run. No manual flash step required.

---

## Recommended Editors

**Mu Editor** is the most beginner-friendly option — it has a built-in serial console and auto-detects CircuitPython boards.

Other options include Thonny, VS Code with extensions, and any plain text editor. The Adafruit guide also documents advanced serial console access on Windows (PuTTY), Mac (screen), and Linux (tio).

---

## Installing Libraries

Most display and peripheral functionality requires additional CircuitPython libraries.

### Method 1: circup (recommended)
`circup` is a CLI tool that installs and updates libraries automatically:
```bash
pip install circup
circup install adafruit_focaltouch
```

### Method 2: Manual
1. Download the Adafruit CircuitPython Library Bundle from [circuitpython.org/libraries](https://circuitpython.org/libraries)
2. Match the bundle version to your CircuitPython version
3. Copy needed `.mpy` files from the bundle into the `lib/` folder on `CIRCUITPY`

### Key libraries for the Qualia S3
| Library | Purpose |
|---|---|
| `dotclockframebuffer` | Built-in — drives the RGB display |
| `framebufferio` | Built-in — wraps framebuffer for displayio |
| `adafruit_focaltouch` | Touch for most displays (FT6206, etc.) |
| `adafruit_cst8xx` | Touch for 2.1" round display (CST826) |

> Always keep CircuitPython and your library bundle versions in sync. Mismatched versions cause `ValueError: Incompatible .mpy file` errors.

---

## The Serial Console & REPL

The serial console lets you see `print()` output and error messages from your code. In Mu, click the **Serial** button. In other editors, use a serial terminal at the board's COM port (115200 baud).

### REPL (Read-Eval-Print Loop)
Press **Ctrl+C** in the serial console to stop running code and enter the REPL — an interactive Python prompt. Useful for testing and debugging.

- `Ctrl+C` — interrupt and enter REPL
- `Ctrl+D` — soft reset, restart code.py
- `Ctrl+A` — paste mode (for pasting multi-line code)

---

## Safe Mode

If your code bricks the board or makes CIRCUITPY read-only, safe mode bypasses `boot.py` and `code.py` without losing your files.

**To enter safe mode (CircuitPython 7.x+):** Press reset, then press it again within the ~1 second window before code starts running. Think of it as a "slow double-click."

In safe mode you can edit files on CIRCUITPY normally, then press reset to exit.

---

## Status LED Blink Codes (CircuitPython 7+)

The Qualia S3 has no NeoPixel, so blink codes won't apply directly, but if you add one via the A0 JST connector:

| Pattern | Meaning |
|---|---|
| 1 green blink (every 5s) | Code finished without error |
| 2 red blinks (every 5s) | Code crashed — check serial for traceback |
| 3 yellow blinks (every 5s) | Safe mode active |
| White | REPL is running |

---

## WiFi

The ESP32-S3 has built-in WiFi. To use it in CircuitPython, create a `settings.toml` file on CIRCUITPY:

```toml
CIRCUITPY_WIFI_SSID = "your-network"
CIRCUITPY_WIFI_PASSWORD = "your-password"
```

Then access it in code:
```python
import wifi
wifi.radio.connect("your-network", "your-password")
```

> BLE is not currently supported in CircuitPython on the S3.

---

## macOS Notes

macOS can write hidden files to CIRCUITPY that consume space and cause corruption. Run these commands once after connecting the board:

```bash
mdutil -i off /Volumes/CIRCUITPY
cd /Volumes/CIRCUITPY
rm -rf .{,_.}{fseventsd,Spotlight-V*,Trashes}
mkdir .fseventsd
touch .fseventsd/no_log .metadata_never_index .Trashes
```

When copying files from the terminal, use `cp -X` to avoid hidden metadata files.
