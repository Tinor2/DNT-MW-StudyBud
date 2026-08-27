#include "persistence.h"

bool persistence_init(void) { return false; }
bool persistence_save(void) { return false; }
void persistence_mark_dirty(void) {}
void persistence_set_suspended(bool suspended) { (void)suspended; }
bool persistence_is_suspended(void) { return false; }
