# DNT-MW-StudyBud

A desk buddy to help you manage work, study, stress and life, in a minimalist and lofi setting.

**Hardware:** ESP32-S3-LCD-2.8C (Waveshare) with ST7701S display, button, rotary encoder, speaker.

## First-time flash (recovery)

If the board doesn't enumerate as a USB device or `idf.py flash` fails with `Failed to write to target RAM (result was 0107: Checksum error)`:

1. **Use a different computer** — the CH343P USB-UART chip on the board may have a driver compatibility issue with the primary machine.
2. On the other machine, install ESP-IDF 6.0: https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/get-started/
3. Clone this repo and flash:
   ```bash
   cd DNT-MW-StudyBud
   . $HOME/esp/esp-idf/export.sh
   idf.py -p /dev/cu.usbmodemXXXX flash
   ```
4. After a successful flash, development can continue on the original machine.

**Important:** Plug into the **USB TO UART** Type-C port on the board (not the native USB port).

## Development

```bash
. $HOME/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash
idf.py -p /dev/cu.usbmodemXXXX monitor
```