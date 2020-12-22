#include "cstructures/init.h"
#include "cstructures/memory.h"

/* ------------------------------------------------------------------------- */
int
cstructures_init(void)
{
    return memory_init();
}

/* ------------------------------------------------------------------------- */
void
cstructures_deinit(void)
{
    memory_deinit();
}
