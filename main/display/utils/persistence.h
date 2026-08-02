#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool persistence_init(void);
bool persistence_save(void);
void persistence_mark_dirty(void);

#ifdef __cplusplus
}
#endif
