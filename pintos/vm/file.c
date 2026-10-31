/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "include/userprog/process.h"
#include "lib/round.h"   
#include "include/userprog/syscall.h"
#include "include/threads/mmu.h"

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

static bool
do_write_back(struct page *page) {
	struct thread *t = thread_current();
	off_t written = 0;
	

	if (page->frame == NULL || !pml4_is_dirty(t->pml4, page->va)) {
		return true;
	} 

	lock_acquire(&filesys_lock);
	written = file_write_at(page->file.mapped_file, page->frame->kva, page->file.read_bytes, page->file.ofs);
	lock_release(&filesys_lock);

	pml4_set_dirty(t->pml4, page->va, false);

	return written == (off_t) page->file.read_bytes;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;

	/* swap out이 발생한 page도 아직 spt에 등록되어 있으므로 destroy가 호출 될 수 있다. 
		 이 때 page는 frame을 가지고 있지 않으므로 아래의 자원 해제를 무시한다. */
	if (page->frame) {
		do_write_back(page);

		pml4_clear_page(thread_current ()->pml4, page->va);
		palloc_free_page(page->frame->kva);
		free(page->frame);
		page->frame = NULL;
	}
}

static void init_file_args (struct file_args *args, struct file *file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, void* start_addr) {
	args->mapped_file = file;
	args->ofs = offset;
	args->read_bytes = read_bytes;
	args->zero_bytes = zero_bytes;
	args->mmap_start_addr = start_addr;
}

static void init_file_page(struct page *page, struct file_args *args) {
	page->file.mapped_file = args->mapped_file;	
	page->file.ofs = args->ofs;
	page->file.read_bytes = args->read_bytes;
	page->file.zero_bytes = args->zero_bytes;
	page->file.mmap_start_addr = args->mmap_start_addr;
}

static bool
lazy_load_file (struct page *page, void *aux) {
	struct file_args *args = (struct file_args *)((struct aux *) aux)->aux_pt;
  void *kva = page->frame->kva;

  lock_acquire(&filesys_lock);
  bool succ = file_read_at (args->mapped_file, kva, args->read_bytes, args->ofs) == (int)args->read_bytes;
  lock_release(&filesys_lock);

  if (succ) memset (kva + args->read_bytes, 0, args->zero_bytes);

	init_file_page(page, args);

	uninit_free_aux((struct aux *) aux);

  return succ;
}

static bool spt_remove_successive_pages(struct supplemental_page_table *spt, void *start, void* end) {
	void *curr = start;
	while (curr < end) {
		struct page *page = spt_find_page(spt, curr);
		if (!page) {
			curr += PGSIZE;
			continue;
		}
		spt_remove_page(spt, page);

		curr += PGSIZE;
	}
	return true;
}

static void *duplicate_file_arg(void * aux) {
	// todo
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
	

		/* TODO: lazy_load_segment에 정보를 넘기기 위해 aux를 설정할 것. */
    struct aux *aux = malloc(sizeof(struct aux));
    if (!aux)
      PANIC("kernal malloc failed.");

    struct file_args *args = malloc(sizeof (struct file_args));
    if (!args)
      PANIC("kernal malloc failed.");

		init_file_args(args, file, offset, read_bytes, zero_bytes, addr);

    aux->aux_pt = args;
    aux->duplicate_func = duplicate_file_arg;

		if (spt_find_page(spt, curr_page) == NULL) {
			bool is_page_allocated = vm_alloc_page_with_initializer(VM_FILE, curr_page, writable, lazy_load_file, aux);

			if (!is_page_allocated) {
				uninit_free_aux(aux);
				
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

static void* get_mmap_starting_addr(struct page *page) {
	switch (VM_TYPE(page->operations->type))
	{
		case VM_UNINIT: {
			if (page_get_type(page) != VM_FILE) {
				return NULL;
			}
			struct file_args *args = (struct file_args *)((struct aux *) page->uninit.aux)->aux_pt;
			return args->mmap_start_addr;
		}
		case VM_FILE: {
			return page->file.mmap_start_addr;
		}
		default:
			return NULL;
	}	
}

static void* get_mmap_file(struct page *page) {
	switch (VM_TYPE(page->operations->type))
	{
		case VM_UNINIT: {
			if (page_get_type(page) != VM_FILE) {
				return NULL;
			}
			struct file_args *args = (struct file_args *)((struct aux *) page->uninit.aux)->aux_pt;
			return args->mapped_file;
		}
		case VM_FILE: {
			return page->file.mapped_file;
		}
		default:
			return NULL;
	}
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = thread_current ()->spt;
	// addr spt 확인
	struct page *page = spt_find_page(spt, addr);
	if (!page) {
		return;
	}

	void *starting_addr = get_mmap_starting_addr(page);
	if (!starting_addr || addr != starting_addr) return;

	struct file *file = get_mmap_file(page);
	if (!file) return;

	// 순회하며 데이터 정리
	uint8_t *curr = (uint8_t *)starting_addr;
	struct page *curr_page = spt_find_page(spt, curr);

	while (curr_page != NULL && starting_addr == get_mmap_starting_addr(curr_page)) {

		spt_remove_page(spt, curr_page);
		
		curr += PGSIZE;
		curr_page = spt_find_page(spt, curr);
	}
	file_close(file);
}
