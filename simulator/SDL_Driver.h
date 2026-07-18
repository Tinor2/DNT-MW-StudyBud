#pragma once

#include "lvgl.h"

#define SIM_LCD_H_RES 480
#define SIM_LCD_V_RES 480

void sdl_driver_init(void);
void sdl_driver_loop(void);
void sdl_driver_present(void);
