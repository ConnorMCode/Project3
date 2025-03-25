#include "threads/synch.h"
#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "threads/palloc.h"

void frame_table_init(void);

void *frame_alloc(enum palloc_flags p_flags);

void frame_free(void *k_address);
