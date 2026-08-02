#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void session_store_init(void);
int  session_store_get_breath_count(void);
void session_store_increment_breath_count(void);
void session_store_set_breath_count(int count);

#ifdef __cplusplus
}
#endif
