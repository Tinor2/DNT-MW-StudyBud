# Display Setup & Configuration (CircuitPython)

## How the Display System Works

The Qualia S3 drives displays using the ESP32-S3's built-in RGB peripheral. The display pipeline is:

1. **Init sequence** — sent once via SPI through the IO expander, configures the display driver IC
2. **Framebuffer** — an 8 MB PSRAM buffer holds the full image
3. **DotClockFramebuffer** — DMA streams the framebuffer to the display continuously at ~30 FPS
4. **FramebufferDisplay** — wraps the framebuffer for use with CircuitPython's `displayio` system

---

## The Basic Display Initialization Pattern

Every display setup follows this same structure:

```python
from displayio import release_displays
release_displays()

import busio
import board
import dotclockframebuffer
from framebufferio import FramebufferDisplay

# 1. Send init sequence via IO expander
board.I2C().deinit()
i2c = busio.I2C(board.SCL, board.SDA)
tft_io_expander = dict(board.TFT_IO_EXPANDER)
# tft_io_expander['i2c_address'] = 0x38  # uncomment for rev B boards
dotclockframebuffer.ioexpander_send_init_sequence(i2c, init_sequence, **tft_io_expander)
i2c.deinit()  # free I2C for other use (e.g. touch)

# 2. Set up pins and timings
tft_pins = dict(board.TFT_PINS)
tft_timings = { ... }  # display-specific, see below

# 3. Create framebuffer and display
fb = dotclockframebuffer.DotClockFramebuffer(**tft_pins, **tft_timings)
display = FramebufferDisplay(fb, auto_refresh=True)
```

> `release_displays()` must be called first to free any previously claimed display resources.

---

## TFT_PINS and TFT_TIMINGS

`board.TFT_PINS` is a pre-defined dict of all the RGB interface GPIO pins for this board. You do not need to define them manually — just use `dict(board.TFT_PINS)`.

`tft_timings` is display-specific and must be looked up for your display. It controls the dot clock frequency and sync pulse widths.

### Timing Parameters Explained

| Parameter | Description |
|---|---|
| `frequency` | Dot clock frequency in Hz. Higher = faster refresh but more prone to distortion |
| `width` / `height` | Active pixel dimensions |
| `hsync_pulse_width` | Horizontal sync pulse duration (in clock cycles) |
| `hsync_front_porch` | Blank clocks before HSYNC |
| `hsync_back_porch` | Blank clocks after HSYNC |
| `vsync_pulse_width` | Vertical sync pulse duration (in line counts) |
| `vsync_front_porch` | Blank lines before VSYNC |
| `vsync_back_porch` | Blank lines after VSYNC |
| `pclk_active_high` | Whether pixel clock samples on rising or falling edge |
| `hsync_idle_low` / `vsync_idle_low` | Sync signal polarity |

### A Note on Dot Clock Frequency

Higher dot clock frequencies increase susceptibility to horizontal shifting artefacts during PSRAM-intensive operations. With IDF 5.1, up to **16 MHz** mostly works reliably. You can lower the frequency to reduce artefacts at the cost of refresh rate.

---

## Display-Specific Init Codes & Timings

### TL021WVC02 — 2.1" 480×480 Round (your display)

```python
init_sequence_tl021wvc02 = bytes((
    0xff, 0x05, 0x77, 0x01, 0x00, 0x00, 0x10,
    0xc0, 0x02, 0x3b, 0x00,
    0xc1, 0x02, 0x0b, 0x02,
    0xc2, 0x02, 0x00, 0x02,
    0xcc, 0x01, 0x10,
    0xcd, 0x01, 0x08,
    # ... (full sequence in PDF or Adafruit learn guide)
    0x11, 0x80, 0x64,
    0x29, 0x80, 0x32,
))

tft_timings = {
    "frequency": 16_000_000,
    "width": 480,
    "height": 480,
    "hsync_pulse_width": 20,
    "hsync_front_porch": 40,
    "hsync_back_porch": 40,
    "vsync_pulse_width": 10,
    "vsync_front_porch": 40,
    "vsync_back_porch": 40,
    "hsync_idle_low": False,
    "vsync_idle_low": False,
    "de_idle_high": False,
    "pclk_active_high": True,
    "pclk_idle_high": False,
}
```

### TL040HDS20 — 4.0" 720×720 Square (no init sequence needed)

```python
init_sequence_tl040hds20 = b""  # no init required

tft_timings = {
    "frequency": 16000000,
    "width": 720,
    "height": 720,
    "hsync_pulse_width": 2,
    "hsync_front_porch": 46,
    "hsync_back_porch": 44,
    "vsync_pulse_width": 2,
    "vsync_front_porch": 16,
    "vsync_back_porch": 18,
    "hsync_idle_low": False,
    "vsync_idle_low": False,
    "de_idle_high": False,
    "pclk_active_high": False,
    "pclk_idle_high": False,
}
```

> Full init sequences and timings for all supported displays are in the PDF guide. Copy them exactly — even small errors can result in no display output or garbled images.

---

## Working with displayio

Once the display is set up, you use standard CircuitPython `displayio` to draw on it:

```python
import displayio

# Create a bitmap (pixel buffer)
bitmap = displayio.Bitmap(display.width, display.height, 65535)

# Create a colour converter (RGB565 works well)
pixel_shader = displayio.ColorConverter(
    input_colorspace=displayio.Colorspace.RGB565
)

# Create a TileGrid (places the bitmap on screen)
tile_grid = displayio.TileGrid(bitmap, pixel_shader=pixel_shader)

# Create a Group and add to display
group = displayio.Group()
group.append(tile_grid)
display.root_group = group
```

To display an image from disk:
```python
bitmap = displayio.OnDiskBitmap("/your-image.bmp")
tile_grid = displayio.TileGrid(bitmap, pixel_shader=bitmap.pixel_shader)
```

> Images must be BMP format. Convert PNGs/JPEGs to BMP before copying to CIRCUITPY.

---

## IO Expander & Display Init

The IO expander (PCA9554A) is the bridge between the ESP32-S3's I2C bus and the display's SPI init interface. The `ioexpander_send_init_sequence()` function handles all of this automatically — you just pass in your init bytes.

If you encounter issues, check:
- Rev B boards may need `tft_io_expander['i2c_address'] = 0x38` (uncomment the line in example code)
- Default expander address is `0x3F`

---

## Touch Display Setup

If your display includes a capacitive touch overlay, it communicates over I2C.

### Determine the I2C Address
Scan for devices from the REPL:
```python
import board
i2c = board.I2C()
while i2c.try_lock(): pass
print(i2c.scan())
```
You'll see the IO expander (0x3F) and the touch controller. The 2.1" round display uses address `0x15`.

### CST826 (2.1" Round Display)
```python
import adafruit_cst8xx
import busio, board

i2c = busio.I2C(board.SCL, board.SDA, frequency=100_000)
ctp = adafruit_cst8xx.Adafruit_CST8XX(i2c)
```

### FocalTouch (most other displays)
```python
import adafruit_focaltouch
import busio, board

i2c = busio.I2C(board.SCL, board.SDA, frequency=100_000)
ctp = adafruit_focaltouch.Adafruit_FocalTouch(i2c, address=0x48)
```

### Reading Touch Input
```python
if ctp.touched:
    for touch in ctp.touches:
        x = touch["x"]
        y = touch["y"]
        print(f"Touch at {x}, {y}")
```

---

## Determining Timings for Unlisted Displays

If your display isn't in the guide, you can derive timings from its datasheet:

1. Find the TCON timing table in the datasheet (look for hdisp, hpw, hbp, hfp, vdisp, vs, vbp, vfp)
2. Map those values to the CircuitPython timing dict fields
3. Start with the typical values, then adjust experimentally if the image is shifted or unstable
4. Lower the `frequency` first if you see horizontal shifting artefacts

You can also convert Arduino_GFX init strings to CircuitPython format using a script described in the full guide.
