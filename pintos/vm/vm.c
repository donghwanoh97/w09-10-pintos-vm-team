/* vm.c: 가상 메모리 객체를 위한 공통 인터페이스. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include "include/threads/vaddr.h"
/* 각 하위 시스템의 초기화 코드를 불러서
 * 가상 메모리 하위 시스템을 초기화한다. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* Project 4용 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* 위의 줄들은 수정하지 말 것. */
	/* TODO: 여기에 코드를 작성. */
}

/* 페이지의 타입을 얻는다. 페이지가 초기화된 뒤 어떤 타입이 될지
 * 알고 싶을 때 유용하다.
 * 이 함수는 이미 완전히 구현되어 있다. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* 도우미 함수들 */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* initializer와 함께 대기 중인(pending) 페이지 객체를 만든다.
 * 페이지를 만들고 싶다면 직접 만들지 말고 이 함수나
 * `vm_alloc_page`를 통해 만들 것. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* upage가 이미 사용 중인지 확인한다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 만들고, VM 타입에 맞는 initializer를 가져온 다음 */
		 
		 /* TODO: uninit_new를 불러 "uninit" 페이지 구조체를 만든다.*/
		 
		 /* TODO: uninit_new를 부른 뒤에 필드를 수정해야 한다. */
		 // page->writable = writable;//

		/* TODO: 페이지를 spt에 넣는다. */
		
	}
err:
	return false;
}

/* spt에서 VA를 찾아 페이지를 돌려준다. 실패하면 NULL을 돌려준다. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	ASSERT(spt && va);
	struct page temp = {.va = va};
	// find 결과 확인 후 엔트리 변환
	struct hash_elem *e = hash_find(&spt->hash, &temp.hash_elem); 
	if (e == NULL)
		return NULL;
	page = hash_entry(e, struct page, hash_elem);
	return page;
}

/* 검증을 거쳐 PAGE를 spt에 넣는다. 주소 유효성 검증은 vm_try_handle_fault에서 처리하는 것으로 구현 */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: 이 함수를 채울 것. */
	/* spt의 hash에 현재 페이지를 넣는다. */
	struct hash_elem *old = hash_insert(&spt->hash, &page->hash_elem);
	if (old == NULL)
		succ = true;
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	ASSERT(spt && page);
	ASSERT(spt_find_page(spt, page->va) == page);
	vm_dealloc_page (page);
	return true;
}

/* 내보낼(evict) struct frame을 고른다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: 내보낼 frame을 고르는 정책은 자유롭게 정한다. */

	return victim;
}

/* 페이지 하나를 내보내고 그 frame을 돌려준다.
 * 실패하면 NULL을 돌려준다. */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: victim을 스왑 아웃하고 비워진 frame을 돌려준다. */

	return NULL;
}

/* palloc()으로 frame을 얻는다. 사용 가능한 페이지가 없으면 페이지를 내보내고
 * 그 frame을 돌려준다. 이 함수는 항상 유효한 주소를 돌려준다. 즉,
 * user pool 메모리가 가득 차면 frame을 내보내서 사용 가능한
 * 메모리 공간을 만든다. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: 이 함수를 채울 것. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 스택을 키운다. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* 쓰기 보호된(write-protected) 페이지에서 난 폴트를 처리한다. */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Anonymous Page의 Lazy Loading, Stack Growth까지 해야 최종 구현 가능. 폴트 유효성 검사에 성공하면 true를 돌려준다. */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	ASSERT(spt);
	/* TODO: 폴트가 유효한지 검사한다 */
	/* 1. 사용자 주소 범위 확인 */
	if (!is_user_vaddr(addr))
		return false;
	/* 2. spt에서 검색하여, 페이지 시작 주소 계산 */
	void *page_addr = pg_round_down(addr); 
	page = spt_find_page(spt, page_addr);
	/* 3. 페이지 부재 폴트인지 확인 */
	if (!not_present)
		return false;
	/* 4. 페이지가 없으면*/
	if (page == NULL) {
		// 스택 확장 조건 검사 
			// 조건을 만족하면 새 스택 페이지 생성
			// return false;
		// 생성한 페이지를 SPT에서 다시 검색
		page = spt_find_page(spt, page_addr); 
		// 페이지 생성 실패 시 false 반환
		if (page == NULL)
			return false;
	}
	/* 5. 쓰기 권한 확인 - page 구조체에 권한을 저장하면 아래 실행 가능 */
	if (write && !page->writable)
		return false;
	/* 5. 프레임 확보 및 매핑 */
	return vm_do_claim_page (page);
}

/* 페이지를 해제한다.
 * 이 함수는 수정하지 말 것. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 할당된 페이지를 확보(claim)한다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: 이 함수를 채울 것 */

	return vm_do_claim_page (page);
}

/* PAGE를 확보하고 MMU를 설정한다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* 서로를 가리키게 연결한다 */
	frame->page = page;
	page->frame = frame;

	/* TODO: 페이지의 VA를 frame의 PA로 매핑하는 페이지 테이블 항목을 넣는다. */

	return swap_in (page, frame->kva);
}
/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}
/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}
/* 새 보조 페이지 테이블을 초기화한다 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	ASSERT(spt);
	hash_init(&spt->hash, page_hash, page_less, NULL);
	
}

/* src의 보조 페이지 테이블을 dst로 복사한다 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
                              struct supplemental_page_table *src) {
    struct hash_iterator i;
    hash_first (&i, &src->hash);

    while (hash_next (&i)) {
        struct hash_elem *e = hash_cur (&i);
        struct page *page = hash_entry(e, struct page, hash_elem);
	    /* 1. 원본 페이지의 타입과 초기화 정보 확인 */
	    /* 2. 타입에 맞게 자식 페이지 생성 및 SPT 등록 */
	    /* 3. 페이지 종류에 따라 초기화 정보 또는 메모리 내용 복사 - 조금 더 고민 /
	    /* 4. 페이지 생성이나 복사에 실패하면 false 반환 */
    }
    return true;
}

/* 해시 요소에 해당하는 페이지를 정리한다. */
static void
page_destroy (struct hash_elem *e, void *aux UNUSED) {
    struct page *page = hash_entry(e, struct page, hash_elem);
    destroy (page);
    free (page);
}


/* 보조 페이지 테이블이 가진 자원을 해제한다 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: 스레드가 가진 보조 페이지 테이블의 내용을 모두 없애고,
	 * TODO: 수정된 내용은 모두 저장소에 다시 써 준다(write-back). */
	hash_destroy(&spt->hash, page_destroy);
}
