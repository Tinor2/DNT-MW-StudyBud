#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sleep_store_init(void);
void sleep_store_seed_demo(void);
void sleep_store_start_session(void);
uint32_t sleep_store_end_session(void);
int sleep_store_get_last_start_hour_min(void);
float sleep_store_get_weekly_avg_hours(void);
void sleep_store_get_last_7_days(uint16_t out_minutes[7]);
void sleep_store_set_history(const uint16_t minutes[7], int count);
void sleep_store_get_history(uint16_t out_minutes[7], int *out_count);

#ifdef __cplusplus
}
#endif
