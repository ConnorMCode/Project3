#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include <stdio.h>

void page_table_init(struct thread *t){
  list_init(&t->page_table);
}

struct page_entry *page_lookup(struct thread *t, void *upage){
  struct list_elem *e;
  for(e = list_begin(&t->page_table); e != list_end(&t->page_table); e = list_next(e)){
    struct page_entry *entry = list_entry(e, struct page_entry, page_elem);
    if (entry->upage == upage){
      return entry;
    }
  }
  return NULL;
}

bool page_insert(struct thread *t, struct page_entry *entry){
  if (page_lookup(t, entry->upage) == NULL){
    list_push_back(&t->page_table, &entry->page_elem);
    return true;
  }
  return false;
}

void page_remove(struct thread *t, void *upage){
  struct page_entry *entry = page_lookup(t, upage);
  if(entry != NULL){
    list_remove(&entry->page_elem);
    free(entry);
  }
}

void page_table_destroy(struct thread *t){
  struct list_elem *e;
  while(!list_empty(&t->page_table)){
    e = list_pop_front(&t->page_table);
    struct page_entry *entry = list_entry(e, struct page_entry, page_elem);
    free(entry);
  }
}
