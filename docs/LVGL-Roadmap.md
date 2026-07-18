# StudyBud — LVGL Integration Roadmap

## Goal

Integrate LVGL v8.2.0 into the StudyBud firmware, replacing the raw `esp_lcd_panel_draw_bitmap()` calls with a proper LVGL UI framework. This is the foundation for all display features.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    LVGL (Core 1)                         │
│  lv_timer_handler() every 10ms                          │
│  ├── Screen Manager (ui_manager.c)                      │
│  │   ├── screen_home.c     (clock + status)             │
│  │   ├── screen_menu.c     (menu wheel navigation)      │
│  │   └── ...future screens                              │
│  ├── Encoder Input (encoder_input.c)                    │
│  │   └── Reads GPIO 4/16/0, routes to active screen     │
│  └── Theme + Fonts (lv_theme.h)                         │
│      └── Lavender + Sage palette, Nunito fonts          │
├─────────────────────────────────────────────────────────┤
│                    LVGL Driver                            │
│  LVGL_Driver.c                                          │
│  ├── lv_init() + display driver registration            │
│  ├── Draw buffers from PSRAM (2x 480x480 RGB565)        │
│  ├── VSYNC semaphore tear avoidance                     │
│  ├── Tick timer (2ms via esp_timer)                     │
│  └── Flush callback → esp_lcd_panel_draw_bitmap()       │
├─────────────────────────────────────────────────────────┤
│                    Hardware                               │
│  ├── ST7701S LCD (480x480, RGB565, V1 driver)           │
│  ├── Rotary Encoder (GPIO 4/16/0)                       │
│  └── PWM Buzzer (GPIO 20, LEDC_TIMER_1)                 │
└─────────────────────────────────────────────────────────┘
```

## Implementation Phases

### Phase 1: Core LVGL Driver ✅ Code written, awaiting build test
- [x] Create directory structure (`main/display/`)
- [x] Create `lv_theme.h` (color palette + font includes)
- [x] Create `LVGL_Driver.c/h` (init, flush, tick, VSYNC)
- [x] Create `encoder_input.c/h` (GPIO reading → LVGL indev)
- [x] Create `ui_manager.c/h` (screen navigation, encoder routing)
- [x] Create `screen_home.c/h` (clock + status, default screen)
- [x] Create `screen_menu.c/h` (menu wheel with existing LVGL widgets)
- [x] Update `main.cpp` to integrate LVGL
- [x] Update `CMakeLists.txt` and `sdkconfig.defaults`
- [x] Add `idf_component.yml` for LVGL dependency
- [x] Create `Kconfig.projbuild` for VSYNC/bounce config
- [ ] **Fix LEDC conflict** — speaker needs TIMER_1/CHANNEL_1 (separate task)
- [x] Create directory structure (`main/display/`)
- [x] Create `lv_theme.h` (color palette + font includes)
- [x] Create `LVGL_Driver.c/h` (init, flush, tick, VSYNC)
- [x] Create `encoder_input.c/h` (GPIO reading → LVGL indev)
- [x] Create `ui_manager.c/h` (screen navigation, encoder routing)
- [x] Create `screen_home.c/h` (clock + status, default screen)
- [x] Create `screen_menu.c/h` (menu wheel with existing LVGL widgets)
- [x] Fix LEDC conflict (speaker → TIMER_1/CHANNEL_1)
- [x] Update `main.cpp` to integrate LVGL
- [x] Update `CMakeLists.txt` and `sdkconfig.defaults`
- [x] Add `idf_component.yml` for LVGL dependency

### Phase 2: Feature Screens (after Phase 1 compiles)
- [ ] `screen_timer.c` — Timer countdown + progress ring
- [ ] `screen_timer_presets.c` — Preset radial list
- [ ] `screen_todos.c` — Todo radial list
- [ ] `screen_water.c` — Water tracker
- [ ] `screen_breathing.c` — Breathing exercise
- [ ] `screen_settings.c` — Quick settings
- [ ] `screen_notifications.c` — Notification history

### Phase 3: Widgets & Polish
- [ ] Toast notification overlay
- [ ] Progress ring widget (using lv_arc)
- [ ] Radial list styling (lv_roller with custom opacity)
- [ ] Menu wheel icon images (48x48 pixel art)
- [ ] Nunito font integration (via lv_font_conv)

## Key Decisions

| Decision | Choice | Rationale |
|---|---|---|
| LVGL version | v8.2.0 | Matches vendor demo, stable, well-documented |
| Draw buffers | 2x full-screen from PSRAM | Smooth rendering, no partial updates |
| VSYNC | Semaphore-based tear avoidance | Prevents tearing on RGB parallel display |
| Encoder input | Custom LVGL indev (not touch) | Reads GPIO directly, routes to active screen |
| Widgets | Existing LVGL widgets styled creatively | Faster than custom canvas widgets |
| Theme | Kconfig-driven (no lv_conf.h) | Matches vendor demo approach |

## File Structure

```
main/
├── display/
│   ├── lvgl_driver/
│   │   ├── LVGL_Driver.c         # LVGL init, flush, tick, VSYNC
│   │   └── LVGL_Driver.h
│   ├── encoder/
│   │   ├── encoder_input.c       # GPIO reading → LVGL input device
│   │   └── encoder_input.h
│   ├── screens/
│   │   ├── screen_home.c         # Clock + status (default)
│   │   ├── screen_home.h
│   │   ├── screen_menu.c         # Menu wheel navigation
│   │   └── screen_menu.h
│   ├── ui_manager.c              # Screen navigation, encoder routing
│   ├── ui_manager.h
│   └── lv_theme.h                # Color palette + font includes
├── main.cpp                      # Updated: calls LVGL init + loop
├── main.c                        # Updated: LEDC on TIMER_1
└── CMakeLists.txt                # Updated: new source files
```

## Hardware Notes

- **LCD**: ST7701S 480x480 RGB565, initialized by V1 driver (`ESP32-S3-LCD-2.8C-V1`)
- **VSYNC**: Registered after `LCD_Init()` returns (panel_handle is extern)
- **Encoder**: GPIO 4 (CLK), GPIO 16 (DT), GPIO 0 (SW/BOOT)
- **Speaker**: GPIO 20, must use LEDC_TIMER_1/CHANNEL_1 (backlight uses TIMER_0/CHANNEL_0)
- **PSRAM**: Octal SPI 80MHz, ~8MB available for draw buffers + LVGL objects
