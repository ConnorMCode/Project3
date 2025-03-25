#include "vm/frame.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "lib/kernel/list.h"

static struct list frame_list;
static struct lock frame_lock;

struct frame_entry {
  void *k_address;
  struct thread *owner;
  struct list_elem frame_elem;
};

void frame_table_init(void){
  list_init(&frame_list);
  lock_init(&frame_lock);
}

void *frame_alloc(enum palloc_flags p_flags){
  if (!(p_flags & PAL_USER)){
    return NULL;
  }

  lock_acquire(&frame_lock);

  void *k_address = palloc_get_page(p_flags);
  if(k_address == NULL){
    lock_release(&frame_lock);
  }

  struct frame_entry *frame = malloc(sizeof(struct frame_entry));
  if (!frame) {
    palloc_free_page(k_address);
    lock_release(&frame_lock);
    return NULL;
  }

  frame->k_address = k_address;
  frame->owner = thread_current();

  list_push_back(&frame_list, &frame->frame_elem);

  lock_release(&frame_lock);
  return k_address;
}

void frame_free(void *k_address){
  lock_acquire(&frame_lock);

  struct list_elem *e;
  for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
    struct frame_entry *frame = list_entry(e, struct frame_entry, frame_elem);

    if(frame->k_address == k_address){
      list_remove(e);
      palloc_free_page(k_address);
      free(frame);
      break;
    }
  }

  lock_release(&frame_lock);
}
