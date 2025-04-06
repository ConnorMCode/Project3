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
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "lib/kernel/list.h"

static struct list frame_list;
static struct lock frames_lock;

void frame_table_init(void){
  list_init(&frame_list);
  lock_init(&frames_lock);
}

struct frame_entry *frame_alloc(struct page_entry *page){
  size_t try;
  struct list_elem *e;

  for(try = 0; try < 3; try++){
    lock_acquire(&frames_lock);

    for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
      struct frame_entry *f_entry = list_entry(e, struct frame_entry, frame_elem);
      if(!lock_try_acquire(&f_entry->lock)){
	continue;
      }

      if(f_entry->k_address == NULL){
	f_entry->k_address = page;
	lock_release(&f_entry->lock);
	lock_release(&frames_lock);
	return f_entry;
      }
      lock_release(&f_entry->lock);
    }

    for(e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
      struct frame_entry *f_entry = list_entry(e, struct frame_entry, frame_elem);

      if (!lock_try_acquire(&f_entry->lock)){
	continue;
      }

      if(f_entry->k_address == NULL){
	f_entry->k_address = page;
	lock_release(&f_entry->lock);
	lock_release(&frames_lock);
	return f_entry;
      }

      if(page_relevant(f_entry->k_address)){
	lock_release(&f_entry->lock);
	continue;
      }

      if(!page_out(f_entry->k_address)){
	lock_release(&f_entry->lock);
	lock_release(&frames_lock);
	return NULL;
      }

      f_entry->k_address = page;
      lock_release(&f_entry->lock);
      lock_release(&frames_lock);
      return f_entry;
    }
    lock_release(&frames_lock);
    timer_msleep(1000);
  }
  return NULL;
}

void frame_lock(struct page_entry *p){
  struct frame_entry *f = p->frame;
  if(f != NULL){
    lock_acquire (&f->lock);
    if(f != p->frame){
      lock_release(&f->lock);
    }
  }
}

void frame_free(struct frame_entry *f){

  ASSERT (lock_held_by_current_thread (&f->lock));

  f->page = NULL;
  lock_release(&f->lock);
}

void frame_unlock(struct frame_entry *f){
  ASSERT(lock_held_by_current_thread (&f->lock));
  lock_release(&f->lock);
}
