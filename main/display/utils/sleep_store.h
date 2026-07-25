#pragma once

#include <stdint.h>

void     sleep_store_init(void);
void     sleep_store_start_session(void);
uint32_t sleep_store_end_session(void);
float    sleep_store_get_weekly_avg_hours(void);
void     sleep_store_get_last_7_days(uint16_t out_minutes[7]);
