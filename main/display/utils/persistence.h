#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool persistence_init(void);
bool persistence_save(void);
void persistence_mark_dirty(void);
void persistence_set_suspended(bool suspended);
bool persistence_is_suspended(void);

#ifdef __cplusplus
}
#endif
