/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include <string.h>
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#include "threads/malloc.h"
#include "filesys/file.h"

/* 2. mmap 등록 */
static bool is_valid_mmap (void *addr, size_t length, struct file *fd_file, off_t offset);
static bool is_page_aligned (void *addr);
static bool is_valid_length (size_t length);
static bool is_valid_offset (off_t offset);
static bool is_user_range (void *addr, size_t length);
static bool map_pages (void *addr, size_t length, int writable, struct file *fd_file, off_t offset);
static off_t bytes_left_in_file (struct file *file, off_t offset);
static size_t bytes_to_read (off_t file_left);
static bool map_one_page (void *addr, int writable, struct file *fd_file, off_t offset, size_t read_bytes);
/* 3. 첫 page fault */
static bool lazy_load_mmap (struct page *page, void *aux);
/* 4. swap */
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void write_back_if_dirty (struct page *page);
/* 5. SPT 복사 */
static void *duplicate_mmap_aux (void *aux);
/* 6. 해제 */
static bool is_valid_munmap (void *addr);
static void file_backed_destroy (struct page *page);


/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* ── 1. 초기화 ── */
/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;	
	struct file_page *info = &page->file; 
	return true; 
}

/* ── 2. mmap: SPT에 등록 ── */
/* 파일을 addr부터 length만큼 페이지 단위로 SPT에 등록한다 (지연 로딩).
* 실제 매핑과 파일 읽기는 첫 page fault 때 일어난다. 성공하면 시작 주소를 반환한다. */
void *
do_mmap (void *addr, size_t length, int writable, struct file *fd_file, off_t offset) {
	if (!is_valid_mmap (addr, length, fd_file, offset)) return NULL;
	if (!map_pages(addr, length, writable, fd_file, offset)) {
		do_munmap (addr); return NULL;
	}
	return addr;
}
/* munmap 인자가 유효한지 검사한다.
*   주소·offset 정렬, 사용자 영역 여부, 파일 길이, 기존 페이지와의 겹침을 확인한다. */
static bool
is_valid_mmap (void *addr, size_t length, struct file *fd_file, off_t offset) {
	if (!is_page_aligned (addr))  return false;
	if (!is_valid_length (length)) return false;
	if (!is_valid_offset (offset)) return false;
	if (!is_user_range (addr, length)) return false;
	lock_acquire (&filesys_lock);	
	off_t flen = file_length (fd_file);
	lock_release (&filesys_lock);
	if (flen == 0) return false;
	for (size_t i = 0; i < length; i += PGSIZE)
		/* mmap을 수행하려면, 이미 SPT에 페이지가 등록되어 있으면 안됨 */
		if (spt_find_page (&thread_current ()->spt, addr + i) != NULL) return false;
	return true; 
}

static bool
is_page_aligned (void *addr) {
	return addr != NULL && pg_ofs (addr) == 0; 
}

static bool
is_valid_length (size_t length) {
	return length != 0;
}

static bool
is_valid_offset (off_t offset) {
	return offset % PGSIZE == 0;
}
static bool is_user_range (void *addr, size_t length) {
	void *end = addr + length - 1;
	return end > addr && is_user_vaddr (addr) && is_user_vaddr (end);
}

/* mmap 영역에서 파일 끝까지 열면서 페이지 한장 SPT에 등록한다 (지연 로딩)*/
static bool
map_pages (void *addr, size_t length, int writable, struct file *fd_file, off_t offset) {
	void *end = addr + length;
	off_t file_end = offset + bytes_left_in_file (fd_file, offset);
	for (; addr < end; addr += PGSIZE, offset += PGSIZE) {
		if (offset >= file_end) break; // 파일 끝까지 매핑
		if (!map_one_page (addr, writable, fd_file, offset, bytes_to_read (file_end - offset))) 
		return false;
	}
	return true;
}
/* 파일에서 offset 이후로 남은 바이트 수 (음수일 수 있음) */
static off_t 
bytes_left_in_file (struct file *file, off_t offset) {
	lock_acquire (&filesys_lock);
	off_t file_left = file_length (file) - offset;
	lock_release (&filesys_lock);
	return file_left; 
}

/* 이번 페이지에서 읽을 바이트 수를 구한다 (0 ~ PGSIZE) */
static size_t
bytes_to_read (off_t file_left) {
	if (file_left <= 0) return 0;
	return file_left < PGSIZE ? file_left : PGSIZE;
}

/* mmap 영역에서 페이지 한장을 SPT에 등록한다.
* 사용자가 fd를 close해도 유지되도록 페이지마다 파일을 따로 연다 
* 아직 메모리에 올리지 않고 SPT에만 등록한다. (지연 로딩) */
static bool
map_one_page (void *addr, int writable, struct file *fd_file, off_t offset, size_t read_bytes) {
	struct file *backing_file = make_backing_file (fd_file);
	struct aux *aux = create_lazy_load_aux (backing_file, offset, read_bytes,
	                                        PGSIZE - read_bytes, duplicate_mmap_aux);
	return vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_mmap, aux);
}

/* src_file을 가리키는 열린 파일(file)을 하나 더 만든다. inode(디스크 실제 파일)은 공유된다 */
struct file *
make_backing_file (struct file *src_file) {
	lock_acquire(&filesys_lock);
	struct file *backing_file = file_reopen(src_file);
	lock_release(&filesys_lock);
	if (backing_file == NULL) 
		PANIC("file_reopen failed");
	return backing_file; 
}

/* ── 3. 첫 page fault: 파일 읽기 ── */
/* 지연 로드 콜백 (mmap용). aux의 파일 정보를 page->file로 옮긴 뒤(aux는 곧 해제됨)
*   lazy_load_segment로 프레임을 채운다. */
static bool
lazy_load_mmap (struct page *page, void *aux) {
	struct lazy_load_info *lazy = ((struct aux *) aux)->aux_pt;
  struct file_page *info = &page->file;
	info->backing_file = lazy-> file;
  info->offset = lazy-> ofs;
  info->read_bytes = lazy-> read_bytes; 
  return lazy_load_segment(page, aux); 
}

/* ── 4. swap ── */
/* Swap in the page by read contents from the file. */
/* 쫓겨났던 페이지를 파일에서 다시 읽어 온다 (swap_out 때 write-back 해 둠) */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *info = &page->file;
	return read_file_into_frame(info->backing_file, info->offset, info->read_bytes, kva); 
}

/* 메모리가 부족할 때 페이지를 잠시 내보낸다.
   수정됐으면 파일에 쓰고(write-back), PTE를 지우고, page와 frame 연결을 끊는다.
   프레임은 eviction 쪽이 재사용하므로 해제하지 않고,
   backing_file은 swap_in에서 다시 읽어야 하므로 닫지 않는다. */
static bool
file_backed_swap_out (struct page *page) {
	struct frame *frame = page->frame;
	if (frame == NULL) return true; // 프레임이 없으면 왜 리턴 트루지. 
	write_back_if_dirty(page); 

  return true;
}

/* 페이지(메모리)가 수정됐으면 파일에 다시 쓰고, dirty 비트를 끈다. */
static void
write_back_if_dirty (struct page *page) {
	struct file_page *info = &page->file;
	if (info->backing_file == NULL) return;
	if (!pml4_is_dirty (page->pml4, page->va)) return;
	lock_acquire(&filesys_lock);
	file_write_at(info->backing_file, page->frame->kva, info->read_bytes, info->offset);
	lock_release(&filesys_lock); 
	pml4_set_dirty(page->pml4, page->va, false); 
}

/* ── 5. SPT 복사 (fork 때 호출) ── */
/* mmap aux의 사본을 만든다 (fork 시 자식용). backing_file은 새로 연다 */
static void *
duplicate_mmap_aux(void *aux) {
	struct lazy_load_info *lazy = ((struct aux *)aux)->aux_pt;
	struct file *backing_file = make_backing_file(lazy->file);	
	if (backing_file == NULL) return NULL; 
	return duplicate_lazy_load_aux(aux, backing_file); 
}

/* ── 6. 해제 ── */
/* 파일 페이지 정리: 프레임이 있으면 write-back 후 반납하고, backing_file은 항상 닫는다. (page는 호출자가 free) */
static void
file_backed_destroy (struct page *page) {
	struct file_page *info = &page->file; 
	if (page->frame != NULL) { 
		write_back_if_dirty(page);
		release_frame(page->frame, page->pml4); 
	} 
	close_backing_file(info->backing_file);
}

/* backing_file을 닫는다. */
void
close_backing_file (struct file *backing_file) {
	lock_acquire(&filesys_lock); 
	file_close(backing_file); 
	lock_release(&filesys_lock); 
}

/* addr부터 연속된 파일 페이지를 SPT에서 빼고 정리한다 (write-back, 프레임 반납, 파일 닫기) */
void
do_munmap (void *addr) {
	if (!is_valid_munmap (addr)) return;
	struct supplemental_page_table *spt = &thread_current ()->spt;
	for (;;) {
		struct page *page = spt_find_page (spt, addr);
		if (!is_mmap_page(page)) 
			break; 
		spt_remove_page (spt, page);
		vm_dealloc_page (page); addr += PGSIZE;	
	} 
}

/* munmap 주소가 유효한지 검사한다: 페이지 정렬 + 파일 페이지 */
static bool
is_valid_munmap (void *addr) {
	if (!is_page_aligned(addr)) return false;
	return is_mmap_page(spt_find_page(&thread_current()->spt, addr));
}

/* mmap으로 만든 페이지인가 (로드 전 UNINIT도 포함). page가 NULL이면 false */
bool
is_mmap_page(struct page *page) {
	return page != NULL && page_get_type (page) == VM_FILE; 
}

