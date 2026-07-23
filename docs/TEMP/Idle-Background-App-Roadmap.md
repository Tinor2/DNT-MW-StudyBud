# Idle Background App — Roadmap

## Overview
A new app screen that displays idle background images with a radial dial selector to browse and choose backgrounds. Uses the same scrolling list system as the Todos and Menu screens.

## Phase 1: Image Pipeline
- [ ] Convert `Tree_idle-background-blue-sky.png` → LVGL C array (RGB565, 480×480)
- [ ] Create `main/assets/idle_bg_tree.c` + header with `LV_IMG_DECLARE(idle_bg_tree)`
- [ ] Add `.c` file to `SRCS` in `main/CMakeLists.txt`
- [ ] Enable image support in simulator `lv_conf.h` (LV_USE_IMG already enabled)
- [ ] ~461KB flash per 480×480 RGB565 image

## Phase 2: Background Selector Screen
- [ ] Create `screen_idle_background.h` / `screen_idle_background.c`
- **Two modes:**
  - **Browse mode** — Radial dial scroll list showing available backgrounds (name + thumbnail)
  - **View mode** — Full-screen display of the selected background
- [ ] Press encoder in browse mode → switch to view mode (full-screen background)
- [ ] Press encoder in view mode → return to browse mode
- [ ] Rotate in browse mode → scroll through backgrounds with radial dial effect

## Phase 3: Background Library
- [ ] Define `bg_item_t` struct: `{ name, lv_img_dsc_t *img }`
- [ ] Start with 1 image (tree), structure allows easy addition
- [ ] Store selection in `session_store` (in-memory, matches existing pattern)

## Phase 4: Integration
- [ ] Add `SCREEN_IDLE_BACKGROUND` to `screen_id_t` enum in `ui_manager.h`
- [ ] Register in `ui_manager.c` (create + event handler)
- [ ] Add `{ LV_SYMBOL_IMAGE, "Idle", SCREEN_IDLE_BACKGROUND }` to menu `menu_items[]`
- [ ] Add to both `main/CMakeLists.txt` and `simulator/CMakeLists.txt`

## Phase 5: Polish
- [ ] Smooth fade transitions between backgrounds
- [ ] Optional: auto-idle timer (return to background after inactivity — stretch goal)

## Technical Notes
- Image format: RGB565 (no alpha) for minimal flash usage
- Display: 480×480 pixels
- LVGL version: 8.2
- No filesystem dependency — images compiled directly into firmware
