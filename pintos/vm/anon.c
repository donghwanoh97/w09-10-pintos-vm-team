/* anon.c: 디스크 이미지가 없는 페이지(익명 페이지, anonymous page)의 구현. */

#include "vm/vm.h"
#include "devices/disk.h"

/* 아래 줄은 수정하지 말 것 */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* 이 구조체는 수정하지 말 것 */
/* 익명 페이지(VM_ANON)용 함수 표 */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* 익명 페이지를 위한 데이터를 초기화한다 (부팅 때 한 번 호출됨) */
void
vm_anon_init (void) {
	/* TODO: swap_disk를 설정한다. */
	swap_disk = NULL;
}

/* 익명 페이지를 초기화한다 (UNINIT → ANON으로 바뀌는 순간) */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 핸들러(함수 표)를 설정한다 */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	anon_page->is_stack = IS_STACK (type);
	return true;
} 

/* 스왑 디스크에서 내용을 읽어 와서 페이지를 스왑 인 한다. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* 내용을 스왑 디스크에 써서 페이지를 스왑 아웃 한다. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* 익명 페이지를 소멸시킨다. PAGE 자체의 메모리는 호출한 쪽에서 해제한다. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
		/* aux는 이미 없는 상태임. 프레임에 대해서 해제를 함 */
		free(page->frame);
		page->frame = NULL;
}