/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "include/userprog/process.h"
#include "lib/round.h"   

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

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
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

static void init_mmap_args (struct mmap_args *args, struct file *file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, void* start_addr) {
	args->file = file;
	args->ofs = offset;
	args->read_bytes = read_bytes;
	args->zero_bytes = zero_bytes;
	args->mmap_start_addr = start_addr;
}

static bool
lazy_load_file (struct page *page, void *aux) {
}

static bool spt_remove_successive_pages(struct supplemental_page_table *spt, void *start, void* end) {
	void *curr = start;
	while (curr < end) {
		struct page *page = spt_find_page(spt, curr);
		if (!page) {
			continue;
		}
		spt_remove_page(spt, page);

		curr += PGSIZE;
	}
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	uint8_t* curr_page = addr;
	off_t file_len = file_length (file);
	size_t remaining = length;   /* 남은 매핑 길이 */
	
	if (!is_range_unmapped(addr, length)) {
		return NULL;
	}

	while (remaining > 0) {
		/* 이번 페이지가 담당하는 매핑 길이 */
		size_t page_len = remaining < PGSIZE ? remaining : PGSIZE;

		/* 이번 페이지 offset부터 파일 끝까지 남은 바이트 (음수면 0) */
		size_t file_left = offset < file_len ? (size_t) (file_len - offset) : 0;

		uint32_t read_bytes = (page_len < file_left) ? page_len : file_left; 
		uint32_t zero_bytes = PGSIZE - read_bytes;
	
		struct mmap_args *args = malloc(sizeof (struct mmap_args));
		if (!args) {
			PANIC("mmap_args malloc");
		}
		init_mmap_args(args, file, offset, read_bytes, zero_bytes, addr);

		if (spt_find_page(spt, curr_page) == NULL) {
			bool is_page_allocated = vm_alloc_page_with_initializer(VM_FILE, curr_page, writable, lazy_load_file, args);

			if (!is_page_allocated) {
				free(args);
				
				// spt에 등록했던 페이지 해제
				spt_remove_successive_pages(spt, addr, curr_page);
				return NULL;
			}
		}

		remaining -= page_len;
		offset += PGSIZE;
		curr_page += PGSIZE;
	}

	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
