# Adafruit Qualia ESP32-S3 — Hardware Overview

## What Is It?

The Adafruit Qualia ESP32-S3 is a development board designed to drive large, high-resolution TTL RGB-666 displays — the kind previously only usable with full microcomputers or cell phone SoCs. The ESP32-S3 is the first affordable microcontroller with a built-in peripheral capable of driving these displays, paired with enough PSRAM to buffer the framebuffer entirely in RAM.

The board ships **without a display** — you source that separately.

---

## Core Specs

| Item | Detail |
|---|---|
| Microcontroller | Espressif ESP32-S3 |
| Logic/Power | 3.3V |
| Flash | 16 MB |
| RAM | 8 MB octal PSRAM |
| Wireless | WiFi + Bluetooth LE (BLE not supported in CircuitPython) |
| USB | USB-C (native USB on ESP32-S3) |
| Refresh Rate | ~30 FPS full screen |
| Programming | CircuitPython or Arduino IDE |

---

## Display Interface

The board uses a **40-pin RGB-666 connector**, which is the secondary standard connector order found on most square, round, and bar displays. This is different from some other 40-pin displays — **do not connect a non-RGB666 display**, as it risks damage due to power pins being in different positions.

Displays connect with metal cable pins facing **toward** the board, with pin 1 furthest from the JST connector.

### Known Compatible Displays

| Display | Resolution | Notes |
|---|---|---|
| TL021WVC02 | 2.1" 480×480 Round | With or without capacitive touch |
| TL028WVC01 | 2.8" Round | — |
| HD40015C40 | 4" 720×720 Round | — |
| TL034WVS05 | 3.4" 480×480 Square | — |
| TL040WVS03 | 4" 480×480 Square | — |
| TL040HDS20 | 4.0" 720×720 Square | — |
| TL032FWV01 | 3.2" 320×820 Bar | — |
| HD371001C | 3.7" 240×960 Bar | — |
| HD458002C40 | 4.58" 320×960 Bar | — |

---

## Available I/O — The Full Picture

GPIO is very scarce on this board. Almost every pin is consumed by the RGB display interface. Here's what's actually available to you:

### Stemma QT / Qwiic Connector
- 4-pin JST I2C connector
- Pullups to 3.3V
- Access via `board.SCL` / `board.SDA` or `board.STEMMA_I2C()`
- **Best option for adding external sensors or I2C devices**

### SPI Pins (usable as digital I/O)
| Pin | Board Name | Arduino |
|---|---|---|
| SCK | `board.SCK` | 5 |
| MISO | `board.MISO` | 6 |
| MOSI | `board.MOSI` | 7 |
| CS | `board.CS` | 15 (10K pull-up) |

These can all be repurposed as digital GPIO if SPI is not needed.

### Analog Pins
- `A0` and `A1` along the bottom edge — usable as analog input or digital I/O
- `A0` also has a 3-pin JST connector (labeled A0) for sensors, NeoPixels, or digital I/O
- A jumper above the JST connector allows switching between 5V and 3V

### PCA9554A IO Expander
- I2C address: `0x3F` (default), adjustable to `0x3B–0x3F` via solder jumpers on reverse
- Used internally for display reset/init, backlight, and the two onboard buttons
- May have remaining pins available depending on your display's needs

### Buttons (onboard)
- **Reset** — top position, restarts firmware (double-tap for bootloader)
- **UP** — middle, connected to IO expander, pulls low when pressed
- **DN** — bottom, connected to IO expander, pulls low when pressed
- **Boot0** — between UP and MCU; hold while pressing reset for ROM bootloader

---

## Backlight Control

- Default: 25 mA (constant current via TPS61169, up to 30V forward voltage)
- Can be increased to up to 200 mA by soldering top jumpers (in 25 mA steps)
- PWM control available by soldering the bottom PWM jumper and using pin `A1`

---

## Debug & Programming

- **USB-C** for power and programming
- **TX0** debug pin — connect a UART RX cable here to read debug output
- **Reset + Boot0** combo for entering ROM bootloader mode

---

## Power

- Powered via USB-C
- 3.3V logic throughout
- Display backlight runs separately at higher voltage (up to 30V via TPS61169 boost converter)
