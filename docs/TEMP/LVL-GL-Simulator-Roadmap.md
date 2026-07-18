# LVGL SDL2 Simulator — Roadmap

## Goal

Test and iterate on StudyBud's LVGL UI on macOS without the ESP32 hardware. The simulator renders the same LVGL screens in an SDL2 window, using keyboard input to simulate the rotary encoder.

## Why

- Rapid UI iteration: no need to flash firmware for every layout/color change
- Design validation: verify the lavender/sage palette, fonts, and screen layouts look right before hardware
- Accessibility: anyone on the team (or stakeholders) can view the UI without dev boards

## Architecture

```
simulator/
├── CMakeLists.txt          # Builds LVGL (static) + SDL2 + our screens
├── main.c                  # SDL2 window init, LVGL tick + loop, keyboard→encoder
├── lv_conf.h               # LVGL config for desktop (16-bit color, no ESP-IDF)
├── mock_esp_log.h          # esp_log macros → printf to stdout
└── SDL_Driver.c/h          # LVGL display + encoder driver via SDL2

Shared source (referenced from main/display/, NOT copied):
├── screens/screen_home.c   # Clock + status (uses esp_log only)
├── screens/screen_menu.c   # lv_list menu (uses esp_log only)
├── ui_manager.c            # Screen navigation (uses esp_log + lv_theme.h)
└── lv_theme.h              # Color palette defines (pure header, no deps)
```

## How It Works

### Display
- LVGL renders to an SDL2 window at 480×480
- SDL_Driver provides `flush_cb` that calls `SDL_UpdateTexture()` to push pixels
- No VSYNC semaphores needed — SDL handles vsync natively

### Input (Encoder Simulation)
| Keyboard Key | Encoder Action |
|---|---|
| Left Arrow | Rotate CCW (`enc_diff = -1`) |
| Right Arrow | Rotate CW (`enc_diff = +1`) |
| Enter | Press (`state = LV_INDEV_STATE_PR`) |
| Escape | Press (alternative) |

### Mock Dependencies
The shared screen code has these ESP-IDF dependencies:
| Dependency | Simulator Replacement |
|---|---|
| `esp_log.h` | `mock_esp_log.h` — `ESP_LOGI/D/W/E` → `printf()` |
| `freertos/FreeRTOS.h` | Not used by screen files |
| `freertos/task.h` | Not used by screen files |
| `esp_heap_caps.h` | Not used by screen files |
| `time.h` | Standard C — uses system clock |

### LVGL Configuration (`lv_conf.h`)
- `LV_COLOR_DEPTH 16` (RGB565, matching ESP32)
- `LV_FONT_DEFAULT &lv_font_montserrat_14`
- Enable Montserrat 14, 16, 20, 48
- `LV_USE_THEME_DEFAULT 1`
- `LV_MEM_CUSTOM 0` (standard malloc for desktop)

## Build Steps

### Prerequisites
```bash
brew install sdl2 cmake
```

### Build & Run
```bash
cd simulator
mkdir -p build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
./studybud_sim
```

## Keyboard Controls Reference

| Key | Action | Screen Behavior |
|---|---|---|
| ← | Rotate CCW | Menu scrolls up, focus changes |
| → | Rotate CW | Menu scrolls down, focus changes |
| Enter | Press select | Menu item selected → switch screen |
| Escape | Back (future) | Return to home (planned) |
| Q | Quit | Closes window, exits |

## Files to Create

| File | Purpose | LOC (est.) |
|---|---|---|
| `simulator/CMakeLists.txt` | Build system | ~40 |
| `simulator/main.c` | Entry point + SDL2 loop | ~80 |
| `simulator/lv_conf.h` | LVGL desktop config | ~150 |
| `simulator/mock_esp_log.h` | Logging shim | ~20 |
| `simulator/SDL_Driver.c` | LVGL display driver via SDL2 | ~60 |
| `simulator/SDL_Driver.h` | Driver header | ~10 |

## Mapping to ESP32 Build

| Concept | ESP32 (main/) | Simulator (simulator/) |
|---|---|---|
| Display driver | `LVGL_Driver.c` (esp_lcd_panel) | `SDL_Driver.c` (SDL2 texture) |
| Input driver | `encoder_input.c` (GPIO) | `SDL_Driver.c` (keyboard) |
| Tick source | `esp_timer` (2ms) | `SDL_GetTicks()` |
| VSYNC | Semaphore-based | SDL handles it |
| Fonts | Montserrat via Kconfig | Montserrat via lv_conf.h |
| Logging | ESP_LOG via esp_log | printf via mock |

## Current Phase

- [x] Phase 0: Planning (this document)
- [ ] Phase 1: Create simulator directory + build system
- [ ] Phase 2: Implement SDL2 display + input driver
- [ ] Phase 3: Integrate shared screen code
- [ ] Phase 4: Build, run, verify screens render correctly
- [ ] Phase 5: Iterate on UI using simulator

## Future Enhancements

- **Mouse input**: Click to simulate encoder press, scroll wheel to rotate
- **Screenshot capture**: Press S to save PNG
- **Resolution scaling**: Scale window to 2x for retina displays
- **Theme switching**: Toggle between light/dark via keyboard
- **WebSocket mock**: Simulate ESP32 state messages for live data preview
