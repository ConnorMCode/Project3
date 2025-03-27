#include <list.h>
#include "filesys/file.h"
#include "threads/thread.h"

enum page_type {
  PAGE_FILE,
  PAGE_ZERO
};

struct page_entry {
  void *upage;
  enum page_type type;
  struct file *file;
  off_t file_offset;
  size_t read_bytes;
  size_t zero_bytes;
  bool writable;
  struct list_elem page_elem;
};

void page_table_init(struct thread *t);
struct page_entry *page_lookup(struct thread *t, void *upage);
bool page_insert(struct thread *t, struct page_entry *entry);
void page_remove(struct thread *t, void *upage);
void page_table_destroy(struct thread *t);
