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
  void *v_address;
  struct thread *owner;
  struct list_elem frame_elem;
  int64_t last_access_time;
};

void frame_table_init(void){
  list_init(&frame_list);
  lock_init(&frame_lock);
}

void *frame_alloc(enum palloc_flags p_flags, void *v_address){
  if (!(p_flags & PAL_USER)){
    return NULL;
  }

  lock_acquire(&frame_lock);

  void *k_address = palloc_get_page(p_flags);
  if(k_address == NULL){
    k_address = frame_evict();
    if (k_address == NULL){
      lock_release(&frame_lock);
      return NULL;
    }
  }

  struct frame_entry *frame = malloc(sizeof(struct frame_entry));
  if (!frame) {
    palloc_free_page(k_address);
    lock_release(&frame_lock);
    return NULL;
  }

  frame->k_address = k_address;
  frame->owner = thread_current();
  frame->v_address = v_address;
  frame->last_access_time = timer_ticks();

  list_push_back(&frame_list, &frame->frame_elem);

  lock_release(&frame_lock);
  return k_address;
}

void *frame_evict(void){

  lock_acquire(&frame_lock);
  
  struct frame_entry *tenant = NULL;
  struct list_elem *e;
  int64_t oldest_time = INT64_MAX;

  for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
    struct frame_entry *frame = list_entry(e, struct frame_entry, frame_elem);

    if (frame->last_access_time < oldest_time) {
      oldest_time = frame->last_access_time;
      tenant = frame;
    }
  }

  if(tenant != NULL){
    if(pagedir_is_dirty(tenant->owner->pagedir, tenant->v_address)){
      swap_write(tenant->k_address, tenant->v_address);
    }

    list_remove(&tenant->frame_elem);
    free(tenant);
    lock_release(&frame_lock);
    return tenant->k_address;
  }

  lock_release(&frame_lock);
  return NULL;
}

void frame_access(void *k_address){
  lock_acquire(&frame_lock);

  for (struct list_elem *e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
    struct frame_entry *frame_entry = list_entry(e, struct frame_entry, frame_elem);

    if (frame_entry->k_address == k_address){
      frame_entry->last_access_time = timer_ticks();
      break;
    }
  }

  lock_release(&frame_lock);
}

void frame_free(void *k_address){
  lock_acquire(&frame_lock);

  struct list_elem *e;
  for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
    struct frame_entry *frame = list_entry(e, struct frame_entry, frame_elem);

    if(frame->k_address == k_address){
      list_remove(&frame->frame_elem);
      palloc_free_page(k_address);
      free(frame);
      break;
    }
  }

  lock_release(&frame_lock);
}
