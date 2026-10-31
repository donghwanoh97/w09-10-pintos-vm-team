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
static void write_back_if_dirty (struct page *page);

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
	struct file_page *info = &page->file;
	return true;
}

/* page->file 정보로 파일 내용을 kva에 읽고, 나머지는 0으로 채운다. (swap_in에서 사용) */
static bool
fill_frame_from_file (struct page *page, void *kva) {
	struct file_page *info = &page->file;
  lock_acquire(&filesys_lock);
  bool succ = file_read_at (info->backing_file, kva, info->read_bytes, info->offset) == (off_t)info->read_bytes;
  lock_release(&filesys_lock);
  if (succ) memset (kva + info->read_bytes, 0, PGSIZE - info->read_bytes);
  	return succ;

}
/* Swap in the page by read contents from the file. */
/* 내보냈던 페이지를 다시 불러 온다 */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	return fill_frame_from_file(page, kva);
}

/* 메모리가 부족할 때 페이지를 잠시 내보낸다.
   수정됐으면 파일에 쓰고(write-back), PTE를 지우고, page와 frame 연결을 끊는다.
   프레임은 eviction 쪽이 재사용하므로 해제하지 않고,
   backing_file은 swap_in에서 다시 읽어야 하므로 닫지 않는다. */
static bool
file_backed_swap_out (struct page *page) {
	struct frame *frame = page->frame;
	if (frame == NULL) 
		return true;
	write_back_if_dirty(page);
	remove_pte_from_pml4(frame, thread_current()->pml4); 
 	unlink_page_and_frame(frame);
	return true;
}

/* aux(lazy load 인자)에 담긴 파일 정보를 page->file로 옮긴다 */
static void
save_file_into_page (struct page *page, void *aux) {
	struct lazy_load_args *args = ((struct aux *) aux)->aux_pt;
  page->file.backing_file = args-> file;
  page->file.offset = args-> ofs;
  page->file.read_bytes = args-> read_bytes;
}

/* 첫 page fault 시: 파일 정보를 page에 저장하고, 파일 내용을 프레임에 읽어 온다 */
static bool
lazy_load_mmap (struct page *page, void *aux) {
	save_file_into_page(page,aux);
  return lazy_load_segment(page, aux);
}

/* src_file을 가리키는 열린 파일(file)을 하나 더 만든다. 
inode(디스크 실제 파일)은 공유된다 */
static struct file *
make_backing_file (struct file *src_file) {
	lock_acquire(&filesys_lock);
	struct file *backing_file = file_reopen(src_file);
	lock_release(&filesys_lock);
	return backing_file;
}

/* mmap aux의 사본을 만든다 (fork 시 자식용). backing_file은 새로 연다 */
static void *
duplicate_mmap_arg(void *aux) {
	struct lazy_load_args *old_arg = ((struct aux *)aux)->aux_pt;
	struct file *backing_file = make_backing_file(old_arg->file);	
	if (backing_file == NULL)
		return NULL;
	return copy_lazy_load_aux(aux, backing_file);
}

/* mmap 인자가 유효한지 검사한다.
   주소·offset 정렬, 사용자 영역 여부, 파일 길이, 기존 페이지와의 겹침을 확인한다. */
static bool
is_valid_mmap (void *addr, size_t length, struct file *fd_file, off_t offset) {
	if (addr == NULL || pg_ofs (addr) != 0 || length == 0 || offset % PGSIZE != 0
	    || !is_user_vaddr (addr) || !is_user_vaddr (addr + length - 1))
		return false;
	lock_acquire (&filesys_lock);
	off_t flen = file_length (fd_file);
	lock_release (&filesys_lock);
	if (flen == 0)
		return false;
	for (size_t i = 0; i < length; i += PGSIZE)
		/* mmap을 수행하려면, 이미 SPT에 페이지가 등록되어 있으면 안됨 */
		if (spt_find_page (&thread_current ()->spt, addr + i) != NULL)
			return false;
	return true;
}

/* 남은 파일 크기로 이번 페이지에서 읽을 바이트 수를 구한다 (0 ~ PGSIZE). */
static size_t
bytes_to_read (off_t file_left) {
	size_t read_bytes = file_left <= 0 ? 0: file_left < PGSIZE ? file_left : PGSIZE;	
	return read_bytes;
}

/* 파일을 addr부터 length만큼 페이지 단위로 SPT에 등록한다 (지연 로딩).
   실제 매핑과 파일 읽기는 첫 page fault 때 일어난다. 성공하면 시작 주소를 반환한다. */
void *
do_mmap (void *addr, size_t length, int writable, struct file *fd_file, off_t offset) {
	if (!is_valid_mmap (addr, length, fd_file, offset))
		return NULL;
	void *start = addr;
	/* offset 이후 파일에 남은 바이트 수 */
	lock_acquire (&filesys_lock);
	off_t file_left = file_length (fd_file) - offset;
	lock_release (&filesys_lock);
	size_t done = 0 ;
	while (done < length) {	
		size_t read_bytes = bytes_to_read(file_left);
		/* 사용자가 fd를 close해도 유지되도록 페이지 전용 파일을 연다 */
		struct file *backing_file = make_backing_file(fd_file);
		struct aux *aux = make_lazy_load_aux(backing_file, offset, read_bytes, PGSIZE - read_bytes, duplicate_mmap_arg);
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_mmap, aux))
			return NULL;
		addr += PGSIZE;  
		offset += PGSIZE;  
		file_left -= read_bytes;  
		done += PGSIZE;
	}
	return start;
}

/* 페이지(메모리)가 수정됐으면 파일에 다시 쓰고, dirty 비트를 끈다. */
static void
write_back_if_dirty (struct page *page) {
	struct thread *cur = thread_current();
	struct file_page *info = &page->file;
	if (!pml4_is_dirty (cur->pml4, page->va)) 
		return;
	lock_acquire(&filesys_lock);
	file_write_at(info->backing_file, page->frame->kva, info->read_bytes, info->offset);
	lock_release(&filesys_lock);
	pml4_set_dirty (cur->pml4, page->va, false);
}

/* backing_file을 닫는다. */
void
close_backing_file (struct file *backing_file) {
	lock_acquire(&filesys_lock);
	file_close(backing_file);
	lock_release(&filesys_lock);
}

/* 파일 페이지 정리: 프레임이 있으면 write-back 후 반납하고, backing_file은 항상 닫는다. (page는 호출자가 free) */
static void
file_backed_destroy (struct page *page) {
	struct file_page *info = &page->file;
	struct thread *cur = thread_current();
	if (page->frame != NULL) {
		write_back_if_dirty(page);
		release_frame(page->frame, cur->pml4);
	}
	close_backing_file(info->backing_file);
}

/* page가 있고 파일 페이지(VM_FILE)인가. UNINIT이면 나중에 될 타입으로 판단한다. */
bool
is_file_backed_page(struct page *page) {
	return page != NULL && page_get_type (page) == VM_FILE;
}

/* addr부터 연속된 파일 페이지를 SPT에서 빼고 정리한다 (write-back, 프레임 반납, 파일 닫기) */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	for (;;) {
		struct page *page = spt_find_page (spt, addr);
		if (!is_file_backed_page(page))
			break;
		spt_remove_page (spt, page);
		vm_dealloc_page (page);
		addr += PGSIZE;
	}
}
