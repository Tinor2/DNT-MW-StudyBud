#pragma once

#include "lvgl.h"

#define SIM_LCD_H_RES 480
#define SIM_LCD_V_RES 480

void sdl_driver_init(void);
void sdl_driver_loop(void);
void sdl_driver_present(void);
void sdl_driver_screenshot(const char *path);

/* When set, sdl_encoder_read stops forwarding encoder events to
   ui_manager (used by scripted runs so the script owns all input). */
void sdl_driver_set_script_mode(bool active);
