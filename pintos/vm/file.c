/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include <string.h>
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#include "threads/malloc.h"
#include "filesys/file.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_mmap (struct page *page, void *aux);

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
	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
}

static bool
lazy_load_mmap (struct page *page, void *aux) {
	struct lazy_load_args *args = ((struct aux *) aux)->aux_pt;
  page->file.mapped_file = args-> file;
  page->file.offset = args-> ofs;
  page->file.read_bytes = args-> read_bytes;
  return lazy_load_segment(page, aux);
}

static void *
duplicate_mmap_arg(void *aux) {
	struct lazy_load_args *old_arg = ((struct aux *) aux)->aux_pt;
	lock_acquire(&filesys_lock);
	struct file *file = file_reopen(old_arg->file);
	lock_release(&filesys_lock);
	return copy_lazy_load_aux(aux, file);
}

static bool
is_valid_mmap (void *addr, size_t length, struct file *file, off_t offset) {
	if (addr == NULL || pg_ofs (addr) != 0 || length == 0 || offset % PGSIZE != 0
	    || !is_user_vaddr (addr) || !is_user_vaddr (addr + length - 1))
		return false;
	lock_acquire (&filesys_lock);
	off_t flen = file_length (file);
	lock_release (&filesys_lock);
	if (flen == 0)
		return false;
	for (size_t i = 0; i < length; i += PGSIZE)
		/* 이미 페이지가 있으면 안됨 */
		if (spt_find_page (&thread_current ()->spt, addr + i) != NULL)
			return false;
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	if (!is_valid_mmap (addr, length, file, offset))
		return NULL;
	void *start = addr;
	lock_acquire (&filesys_lock);
	off_t file_left = file_length (file) - offset;
	lock_release (&filesys_lock);
	size_t done = 0;
	while (done < length) {
		size_t read_bytes = file_left <= 0 ? 0
											: file_left < PGSIZE ? file_left : PGSIZE;
		lock_acquire (&filesys_lock);
		struct file *reopened = file_reopen (file);  
		lock_release (&filesys_lock);
		struct aux *aux = make_lazy_load_aux(reopened, offset, read_bytes, PGSIZE - read_bytes, duplicate_mmap_arg);
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_mmap, aux))
			return NULL;
		addr += PGSIZE;
		offset += read_bytes;
		file_left -= read_bytes;
		done += PGSIZE;
	}
	return start;
}

/* Destroy the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	struct thread *cur = thread_current();
	if (page->frame != NULL) {
		if (pml4_is_dirty (cur->pml4, page->va)) {
			lock_acquire(&filesys_lock);
			file_write_at(file_page->mapped_file, page->frame->kva, file_page->read_bytes, file_page->offset);
			lock_release(&filesys_lock);		
		}
		vm_remove_frame(page->frame, cur->pml4);
	}
	lock_acquire(&filesys_lock);
	file_close(file_page->mapped_file);
	lock_release(&filesys_lock);
	}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	for (;;) {
		struct page *page = spt_find_page (spt, addr);
		if (page == NULL || page_get_type (page) != VM_FILE)
			break;
		spt_remove_page (spt, page);
		addr += PGSIZE;
	}
}
