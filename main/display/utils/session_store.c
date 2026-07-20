#include "session_store.h"

static int breath_count = 0;

void session_store_init(void)
{
    breath_count = 0;
}

int session_store_get_breath_count(void)
{
    return breath_count;
}

void session_store_increment_breath_count(void)
{
    breath_count++;
}
