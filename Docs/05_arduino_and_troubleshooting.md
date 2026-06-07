# Arduino IDE & Troubleshooting Reference

## Arduino IDE Setup

The Qualia S3 can be programmed with the Arduino IDE as an alternative to CircuitPython.

### 1. Install Arduino IDE
Download from [arduino.cc](https://arduino.cc) — version 2.x is recommended.

### 2. Add ESP32 Board Support
In Arduino IDE, go to **File → Preferences** and add the following to "Additional Board Manager URLs":
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Then go to **Tools → Board → Boards Manager**, search for `esp32` by Espressif, and install it.

### 3. Select the Board
Go to **Tools → Board → ESP32 Arduino** and select the appropriate ESP32-S3 board.

### 4. Install Required Libraries
Go to **Sketch → Include Library → Manage Libraries** and install:
- `Adafruit GFX Library`
- `Adafruit BusIO`
- Any display-specific libraries referenced in your sketch

---

## Uploading to the Board

### Normal Upload
Select the correct COM port under **Tools → Port** and click Upload.

### If Upload Fails — Enter ROM Bootloader Mode
Hold the **Boot0** button while pressing **Reset**, then release both. The board enters ROM bootloader mode and the IDE can flash it.

Alternatively, from the Arduino IDE menu: hold Boot0, click Upload, release Boot0 when upload starts.

---

## Arduino Rainbow Demo

Adafruit provides a pre-built Arduino rainbow demo as a UF2 file — useful for quickly testing hardware without writing any code:

1. Visit the Adafruit learn guide for the Qualia S3
2. Find the "Arduino Rainbow Demo" section and download the appropriate UF2 for your display
3. Double-tap reset to enter bootloader, drag the UF2 to `TFT_S3BOOT`

This is also a good first test if your CircuitPython display isn't working — it rules out hardware issues.

---

## Arduino Touch Display Usage

Touch works similarly to CircuitPython — over I2C. Two controller types are supported:

**FocalTouch (FT6206 etc.)** — used on most larger displays
```cpp
#include <Adafruit_FocalTouch.h>
Adafruit_FocalTouch ctp = Adafruit_FocalTouch();
ctp.begin(0x48);  // adjust address if needed
```

**CST826** — used on the 2.1" round display
```cpp
#include <Adafruit_CST8XX.h>
Adafruit_CST8XX cst;
cst.begin();
```

---

## Troubleshooting

### Display Shows Nothing
- Check that your init sequence is correct for your specific display — even one wrong byte can cause no output
- Ensure you're calling `release_displays()` at the start
- Confirm your cable is connected correctly (metal pins toward board, pin 1 away from JST)
- Try the Rainbow Demo UF2 to test hardware independently of your code
- Rev B boards: uncomment the `i2c_address = 0x38` line in your expander setup

### Display Shows Garbled/Shifted Image
- Lower the `frequency` in your `tft_timings` — try 12 MHz or 10 MHz
- Adjust front/back porch values — start with the typical values from the datasheet
- Horizontal shifting during PSRAM activity is a known limitation; reducing dot clock frequency helps

### CIRCUITPY Drive Not Appearing
- Try a different USB cable — charge-only cables are common and won't work
- Try a different USB port or hub
- On Windows, anti-virus software (BitDefender, Kaspersky, Norton) can block the drive — set exceptions or temporarily disable
- Samsung Magician and AIDA64 are also known to interfere on Windows
- Try double-tapping reset to enter bootloader, then re-flashing CircuitPython

### code.py Restarts Constantly
Backup software, anti-virus, or disk-checking apps can write to CIRCUITPY and trigger constant reloads. Disable auto-reload if needed:
```python
import supervisor
supervisor.runtime.autoreload = False
```

### ValueError: Incompatible .mpy file
Your library bundle version doesn't match your CircuitPython version. Download the matching bundle from [circuitpython.org/libraries](https://circuitpython.org/libraries) and replace your `lib/` folder contents.

### ImportError: No module named '...'
The required library isn't in your `lib/` folder. Install it via `circup install library_name` or copy it manually from the bundle.

### Touch Not Working
- Scan I2C to confirm the touch controller address: `i2c.scan()`
- The 2.1" round display uses `0x15`, others commonly use `0x48`
- Ensure you're using the correct library (CST8XX vs FocalTouch)
- I2C must be initialised after the display init sequence — the expander call uses I2C first

### Out of Memory
The ESP32-S3 has 8 MB PSRAM but a relatively small code heap. Tips:
- Use `.mpy` compiled library files instead of `.py` source files
- Call `gc.collect()` periodically
- Check free memory with: `import gc; print(gc.mem_free())`
- Avoid large Python lists; prefer `bytearray` for raw data

### Board Locked Up / Boot Looping
Enter safe mode (slow double-tap of reset) to access CIRCUITPY and remove or fix problematic code. If safe mode doesn't work, completely erase and re-flash CircuitPython.

To erase from the REPL:
```python
import storage
storage.erase_filesystem()
```

---

## Restoring the Bootloader

If the UF2 bootloader is missing or broken:

1. Download the UF2 bootloader `.bin` file for the Qualia S3 from Adafruit
2. Enter ROM bootloader mode (hold Boot0 + press Reset)
3. Use the [Adafruit WebSerial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/) in Chrome to flash it
4. Or use `esptool.py` from the command line (advanced)

After flashing, reset the board and double-tap reset to confirm `TFT_S3BOOT` appears.

---

## WiFi / Network Issues

- Only WiFi is supported in CircuitPython — not BLE
- Credentials go in `settings.toml` (not hardcoded in `code.py`)
- IPv6 is supported in CircuitPython 8+
- For Adafruit IO integration, use the `adafruit_io` library and set up your feed keys in `settings.toml`

---

## Getting Help

- **Adafruit Discord** — `#help-with-circuitpython` and `#help-with-projects` channels
- **Adafruit Forums** — [forums.adafruit.com](https://forums.adafruit.com)
- **CircuitPython Docs** — [docs.circuitpython.org](https://docs.circuitpython.org)
- **Adafruit Learn Guide** — [learn.adafruit.com/adafruit-qualia-esp32-s3-for-rgb666-displays](https://learn.adafruit.com/adafruit-qualia-esp32-s3-for-rgb666-displays)

Always run the latest stable CircuitPython and matching library bundle before reporting bugs.
