/* file.c: 메모리에 올라온 파일 객체(mmap된 객체)의 구현. */

#include "vm/vm.h"

/* 아래 함수 표에 넣기 위해 함수들을 먼저 선언(프로토타입)해 둔다 */
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* 이 구조체는 수정하지 말 것 */
/* 파일 기반 페이지(VM_FILE)용 함수 표 */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* 파일 기반 VM의 초기화 함수 (부팅 때 한 번 호출됨) */
void
vm_file_init (void) {
}

/* 파일 기반 페이지를 초기화한다 (UNINIT → FILE로 바뀌는 순간) */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 핸들러(함수 표)를 설정한다 */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* 파일에서 내용을 읽어 와서 페이지를 스왑 인 한다. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 내용을 파일에 다시 써서(write-back) 페이지를 스왑 아웃 한다. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 파일 기반 페이지를 소멸시킨다. PAGE 자체의 메모리는 호출한 쪽에서 해제한다. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* mmap을 수행한다 */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
}

/* munmap을 수행한다 */
void
do_munmap (void *addr) {
}