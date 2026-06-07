# Adding Peripherals — Buttons, Encoders, Audio & LEDs

## The GPIO Reality

Nearly every GPIO on the ESP32-S3 is consumed by the RGB display interface. The following pins are your entire "free" budget:

| Resource | Pins Available | Best Use |
|---|---|---|
| Stemma QT (I2C) | SCL, SDA | I2C sensors, encoders, touch |
| SPI pins | SCK, MISO, MOSI, CS | Digital I/O or I2S audio |
| Analog pins | A0, A1 | Analog sensors, LEDs, PWM |
| JST A0 connector | A0 | NeoPixels, sensors |
| IO Expander (I2C) | Remaining expander pins | Buttons, simple outputs |

Plan your project around these constraints before purchasing components.

---

## External Push Button

**Recommended approach: SPI pin as digital GPIO**

The SPI pins (SCK, MISO, MOSI, CS) can be used as regular digital input pins if you're not using SPI for other hardware. This gives you true interrupt capability — the most responsive option for a button.

```python
import board
import digitalio

button = digitalio.DigitalInOut(board.SCK)  # or MISO, MOSI, CS
button.direction = digitalio.Direction.INPUT
button.pull = digitalio.Pull.UP  # button connects pin to GND

while True:
    if not button.value:  # active low
        print("Button pressed!")
```

> CS already has a 10K pull-up resistor on the board, making it a convenient choice for a button.

**Alternative: IO Expander**

If you need the SPI pins for something else, use the IO expander over I2C. This adds a small polling overhead but is perfectly fine for button inputs.

```python
import board
import busio
from adafruit_pca9554 import PCA9554

i2c = busio.I2C(board.SCL, board.SDA)
expander = PCA9554(i2c, address=0x3F)

# Configure a pin as input
expander.get_pin(4).switch_to_input()

while True:
    if not expander.get_pin(4).value:
        print("Button pressed!")
```

> The two onboard UP/DN buttons are already connected to the IO expander. If those suit your needs, no wiring is needed at all.

---

## I2C Rotary Encoder (360° Input)

An I2C rotary encoder module is the cleanest option for 360° continuous rotation — no GPIO pins consumed, plug directly into Stemma QT.

**Recommended module:** Adafruit I2C QT Rotary Encoder with NeoPixel (product #4991) or similar.

### Wiring
Connect to the Stemma QT port — that's it. The encoder communicates over I2C.

### Code (Adafruit Seesaw-based encoder)
```python
import board
from adafruit_seesaw import seesaw, rotaryio, digitalio

i2c = board.STEMMA_I2C()
seesaw_device = seesaw.Seesaw(i2c, addr=0x36)

encoder = rotaryio.IncrementalEncoder(seesaw_device)
button = digitalio.DigitalIO(seesaw_device, 24)

last_position = 0
while True:
    position = encoder.position
    if position != last_position:
        print(f"Encoder position: {position}")
        last_position = position
    if not button.value:
        print("Encoder button pressed!")
```

### I2C Address Conflicts
The IO expander is at `0x3F` by default. Most I2C encoders default to `0x36`. The touch controller on the 2.1" display is at `0x15`. These should not conflict, but if you add more I2C devices, scan for conflicts first:

```python
import board
i2c = board.I2C()
while i2c.try_lock(): pass
print([hex(addr) for addr in i2c.scan()])
```

If the IO expander conflicts with another device, it can be moved to `0x3B–0x3E` via solder jumpers on the back of the board.

---

## I2S Speaker / Audio Output

The SPI pins can be repurposed as I2S audio output when SPI is not needed for other hardware.

**Required hardware:** An I2S amplifier board such as the MAX98357A (mono, common and cheap) or UDA1334A (stereo). Connect a small speaker to the amp's output.

### Wiring (MAX98357A example)
| Amp Pin | Qualia Pin |
|---|---|
| BCLK | MOSI (board.MOSI) |
| LRC / WS | SCK (board.SCK) |
| DIN | MISO (board.MISO) |
| GND | GND |
| VIN | 3.3V or 5V |

### Code
```python
import audiobusio
import audiocore
import board

audio = audiobusio.I2SOut(
    bit_clock=board.MOSI,
    word_select=board.SCK,
    data=board.MISO
)

# Play a wave file
with open("/sound.wav", "rb") as f:
    wave = audiocore.WaveFile(f)
    audio.play(wave)
    while audio.playing:
        pass
```

> Audio files must be WAV format, mono or stereo, 16-bit PCM. Keep sample rates moderate (22050 Hz works well).

> **Pin conflict note:** If you use I2S audio, you cannot also use those SPI pins for a button. Plan your pin usage before wiring.

---

## Adding an LED

### NeoPixel via JST A0 Connector (recommended)
The 3-pin JST A0 connector is perfect for a NeoPixel LED. A single NeoPixel gives you full RGB colour control on one data pin.

```python
import board
import neopixel

pixel = neopixel.NeoPixel(board.A0, 1, brightness=0.3)
pixel[0] = (255, 0, 0)   # Red
pixel[0] = (0, 255, 0)   # Green
pixel[0] = (0, 0, 255)   # Blue
```

The JST connector defaults to 5V. Cut and re-solder the jumper above it if you need 3V instead.

### Plain LED via A0 or A1
A regular LED with a 330Ω resistor can be connected to A0 or A1:

```python
import board
import digitalio

led = digitalio.DigitalInOut(board.A1)
led.direction = digitalio.Direction.OUTPUT
led.value = True   # on
led.value = False  # off
```

For PWM brightness control:
```python
import board
import pwmio

led = pwmio.PWMOut(board.A1, frequency=5000)
led.duty_cycle = 32768  # 50% brightness (0–65535)
```

---

## Putting It All Together — Recommended Pin Plan

For a project with an I2C encoder, external button, I2S speaker, and NeoPixel LED:

| Component | Connection | Pins Used |
|---|---|---|
| 2.1" Round Display | 40-pin FPC | Dedicated TFT pins |
| Capacitive Touch (CST826) | I2C (internal) | SCL, SDA |
| IO Expander (PCA9554A) | I2C | SCL, SDA |
| I2C Rotary Encoder | Stemma QT | SCL, SDA |
| External Button | CS pin as digital input | CS (GPIO15) |
| I2S Amplifier + Speaker | SPI pins as I2S | MOSI, SCK, MISO |
| NeoPixel LED | JST A0 | A0 |

This configuration uses all available free resources with no conflicts.

> **Important:** If using I2S audio, the MOSI/SCK/MISO pins are occupied — you can't also use them for SPI peripherals. The button must go on CS or an IO expander pin in this case.

---

## I2C Bus Speed

The default I2C bus speed is 100 kHz. For display init sequences, 400 kHz is often faster:

```python
i2c = busio.I2C(board.SCL, board.SDA, frequency=400_000)
```

After sending the display init sequence, call `i2c.deinit()` then reinitialise at a lower speed if your touch controller requires it. The CST826 works at 100 kHz.
