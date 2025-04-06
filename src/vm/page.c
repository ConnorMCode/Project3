#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include <string.h>
#include <stdio.h>

#define STACK_MAX (1024 * 1024)

void page_table_init(struct thread *t){
  list_init(&t->page_table);
}

struct page_entry *page_from_frame(void *kaddr){
  struct list_elem *e;
  struct thread *t = thread_current();
  for(e = list_begin(&t->page_table); e != list_end(&t->page_table); e = list_next(e)){
    struct page_entry *entry = list_entry(e, struct page_entry, page_elem);
    if (entry->frame == kaddr){
      return entry;
    }
  }

  if (kaddr < PHYS_BASE) {
    void *upage = pg_round_down(kaddr);

    uintptr_t phys_base = (uintptr_t)PHYS_BASE;
    uintptr_t stack_max = (uintptr_t)STACK_MAX;
    uintptr_t upage_addr = (uintptr_t)upage;
    uintptr_t kaddr_addr = (uintptr_t)kaddr;

    if (upage_addr > phys_base - stack_max && kaddr_addr >= phys_base - stack_max) {
      return page_allocate(upage, true);
    }
  }

  return NULL;
}

struct page_entry *page_lookup(struct thread *t, const void *upage){
  struct list_elem *e;
  for(e = list_begin(&t->page_table); e != list_end(&t->page_table); e = list_next(e)){
    struct page_entry *entry = list_entry(e, struct page_entry, page_elem);
    if (entry->upage == upage){
      return entry;
    }
  }

  return NULL;
}

bool page_in(void *fault_addr){
  struct thread *t = thread_current();
  void *upage = pg_round_down(fault_addr);

  // Find the page in the thread's page list
  struct page_entry *p = NULL;
  for (struct list_elem *e = list_begin(&t->page_table); e != list_end(&t->page_table); e = list_next(e)) {
    struct page_entry *entry = list_entry(e, struct page_entry, page_elem);
    if (entry->upage == upage) {
      p = entry;
      break;
    }
  }

  if (p == NULL) {
    printf("Page for fault address %p not found\n", fault_addr);
    return false;
  }

  // Allocate a frame
  struct frame_entry *frame = frame_alloc(p);
  if (frame == NULL) {
    printf("Failed to allocate frame for page %p\n", upage);
    return false;
  }

  // Load the data into the frame
  if (p->in_swap) {
    swap_in(p);
  } else if (p->file != NULL) {
    file_seek(p->file, p->file_offset);
    int bytes_read = file_read(p->file, frame->base, p->read_bytes);
    if (bytes_read != (int)p->read_bytes) {
      printf("Error reading from file for page %p: expected %zu, got %d\n",
	     upage, p->read_bytes, bytes_read);
      frame_free(frame);
      return false;
    }
    memset((uint8_t *)frame + p->read_bytes, 0, p->zero_bytes);
  } else {
    memset(frame, 0, PGSIZE);
  }

  // Install the page into the page directory
  if (!pagedir_set_page(t->pagedir, upage, frame, p->writable)) {
    printf("pagedir_set_page failed for %p\n", upage);
    frame_free(frame);
    return false;
  }

  p->frame = frame;
  return true;
}

bool page_out(struct page_entry *p){
  bool dirty;
  bool ok = false;

  pagedir_clear_page(p->thread->pagedir, (void *) p->upage);

  dirty = pagedir_is_dirty(p->thread->pagedir, (const void *) p->frame);

  if(!dirty){
    ok = true;
  }

  if(p->file == NULL){
    ok = swap_out(p);
  }else{
    if(dirty){
      ok = file_write_at(p->file, (const void *)p->frame->base, p->read_bytes, p->file_offset);
    }
  }

  if(ok){
    p->frame = NULL;
  }
  return ok;
}

bool page_relevant(struct page_entry *p){
  if(pagedir_is_accessed(p->thread->pagedir, p->upage)){
    pagedir_set_accessed(p->thread->pagedir, p->upage, false);
  }
  return pagedir_is_accessed(p->thread->pagedir, p->upage);
}

struct page_entry *page_allocate(void *uaddr, bool writable){
  struct thread *t = thread_current();
  struct page_entry *p = palloc_get_page(PAL_ZERO);

  if(p != NULL){
    p->upage = pg_round_down(uaddr);
    p->writable = writable;
    p->frame = NULL;

    p->in_swap = false;
    p->swap_index = (size_t) -1;

    p->file = NULL;
    p->file_offset = 0;
    p->read_bytes = 0;
    p->zero_bytes = 0;

    p->type = PAGE_ZERO;
    p->thread = t;

    list_push_back(&t->page_table, &p->page_elem);
  }
  return p;
}

void page_deallocate(void *uaddr){
  struct page_entry *p = page_lookup(thread_current(), uaddr);
  frame_lock(p);
  if(p->frame){
    struct frame_entry *f = p->frame;
    if(p->file && p->writable){
      page_out(p);
    frame_free(f);
    }
  }
  list_remove(&p->page_elem);
  free(p);
}

bool page_lock(const void *uaddr, bool write){
  struct page_entry *p = page_lookup(thread_current(), uaddr);
  if(p == NULL ||(!p->writable && write)){
    return false;
  }
  frame_lock(p);
  if(p->frame == NULL){
    return(page_in(p) && pagedir_set_page(thread_current()->pagedir, p->upage, p->frame->base, p->writable));
  }else{
    return true;
  }
}

void page_unlock(const void *uaddr){
  struct page_entry *p = page_lookup(thread_current(), uaddr);
  ASSERT (p != NULL);
  frame_unlock(p->frame);
}

void page_table_destroy(struct thread *t){
  struct list_elem *e;
  while(!list_empty(&t->page_table)){
    e = list_pop_front(&t->page_table);
    struct page_entry *entry = list_entry(e, struct page_entry, page_elem);
    free(entry);
  }
}
