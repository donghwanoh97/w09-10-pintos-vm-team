/* file.c: Implementation of memory backed file object (mmaped object). */
#include <string.h>
#include "vm/vm.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "userprog/syscall.h"

#define AUX_TO_ARGS(aux_) ((struct load_file_args *)((struct aux *) (aux_))->aux_pt)

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_file (struct page *page, void *aux);
static void clear_alloc_failed_pages(void *start, void *failed);
static void *duplicate_load_file_arg(void * aux);
static void *malloc_with_panic(size_t size);

static void check_swap_out(struct page *page);
static void check_mm_file_free(struct mmap_file *mm_file);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

  struct load_file_args *args = AUX_TO_ARGS(page->uninit.aux);
	
  struct file_page *file_page = &page->file;
  file_page->mmap_file = args->file;
  file_page->offset = args->ofs;
  file_page->zero_bytes = args->zero_bytes;
  file_page->head_page = spt_find_page(&thread_current()->spt, args->mmap_addr);

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;

}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
  struct file *file = file_page->mmap_file->file;

  ASSERT(file != NULL);
  off_t write_bytes = PGSIZE - file_page->zero_bytes;
  lock_acquire(&filesys_lock);
  file_write_at(file, page->va, write_bytes, file_page->offset);
  lock_release(&filesys_lock);
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
  check_swap_out(page);

  struct mmap_file *mmap_file = page->file.mmap_file;
  
  mmap_file->count --;
  check_mm_file_free(mmap_file);
}

static void
check_swap_out(struct page *page){
  // TODO : 현재 스레드의 pml4를 사용하는 것이 항상 안전한지 swap 구현 후 확인하기
  if (pml4_is_dirty(thread_current()->pml4, page->va)){
    file_backed_swap_out(page);
  }
}

static void
check_mm_file_free(struct mmap_file *mm_file){
  if (mm_file->count == 0){
    lock_acquire(&filesys_lock);
    file_close(mm_file->file);
    lock_release(&filesys_lock);

    free(mm_file);
  }
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
  struct file *file, off_t offset) {
  ASSERT(pg_ofs(addr) == 0)
  ASSERT(file != NULL);

  void *mmap_addr = addr;

  struct file *file_copy = reopen_file(file);
  struct mmap_file *new_file = malloc_with_panic(sizeof(struct mmap_file));
  new_file->file = file_copy;
  new_file->count = 0;

  while(length > 0){
    size_t file_read_bytes = length < PGSIZE ? length : PGSIZE;
    size_t file_zero_bytes = PGSIZE - file_read_bytes;

    struct aux *aux = malloc(sizeof(struct aux));
    if (!aux)
      PANIC("kernal malloc failed");
    
    struct load_file_args *args = malloc(sizeof (struct load_file_args));
    if (!args)
      PANIC("kernal malloc failed.");
    
    args->file = new_file;
    args->ofs = offset;
    args->read_bytes = file_read_bytes;
    args->zero_bytes = file_zero_bytes;
    args->mmap_addr = mmap_addr;

    aux->aux_pt = args;
    aux->duplicate_func = duplicate_load_file_arg;

    if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_file, aux)){
      clear_alloc_failed_pages(mmap_addr, addr);
      return NULL;
    }

    new_file->count++;
    offset += file_read_bytes;
    length -= file_read_bytes;
    addr += PGSIZE;
  }

  return mmap_addr;
}

static bool
lazy_load_file (struct page *page, void *aux){
  struct load_file_args *args = AUX_TO_ARGS(aux);
  void *kva = page->frame->kva;

  lock_acquire(&filesys_lock);
  bool succ = file_read_at(args->file->file, kva, args->read_bytes, args->ofs) == (int) args->read_bytes;
  lock_release(&filesys_lock);

  if (succ) memset(kva + args->read_bytes, 0, args->zero_bytes);

  return succ;
}

/* Do the munmap */
void
do_munmap (void *addr) {
  struct supplemental_page_table *spt = &thread_current()->spt;
  struct page *page = spt_find_page(spt, addr);

  int count = page->file.mmap_file->count;
  for (int i = 0; i < count; i++){
    spt_remove_page(spt, page);

    addr += PGSIZE;
    page = spt_find_page(spt, addr);
  }
}

bool
is_mmap_addr(void *addr){
  struct page *page = spt_find_page(&thread_current()->spt, addr);
  if (!page) return false;
  
  return page == page->file.head_page;
}

struct file *
reopen_file(struct file * file){
  ASSERT(file != NULL);
  
  lock_acquire(&filesys_lock);
  struct file *new_file = file_reopen(file);
  lock_release(&filesys_lock);
  
  if (!new_file)
    PANIC("file reopen failed.");
  
  return new_file;
}

static void clear_alloc_failed_pages(void *start, void *failed){
  ASSERT(pg_ofs(start) == 0);
  ASSERT(pg_ofs(failed) == 0);
  ASSERT((uint64_t)start < (uint64_t)failed);

  void *cur = start;
  struct supplemental_page_table *spt = &thread_current()->spt;
  while(cur < failed){
    struct page *to_remove = spt_find_page(spt, cur);
    if (to_remove) spt_remove_page(spt, to_remove);

    cur += PGSIZE;
  }
}

static void *
duplicate_load_file_arg(void * aux){
  struct aux *src = (struct aux *)aux;
  struct load_file_args *old_arg = (struct load_file_args *)src->aux_pt;

  struct aux *dst = malloc_with_panic(sizeof(struct aux));  
  struct load_file_args *new_arg = malloc_with_panic(sizeof (struct load_file_args));
  
  dst->aux_pt = new_arg;
  dst->duplicate_func = src->duplicate_func;

  memcpy(new_arg, old_arg, sizeof(struct load_file_args));

  //new_arg->file = new_file; TODO : get file;

  return (void *)dst;
}

static void *
malloc_with_panic(size_t size){
  void *allocated = malloc(size);
  if (!allocated)
    PANIC("kernal malloc failed.");

  return allocated;
}

