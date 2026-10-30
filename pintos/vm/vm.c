/* vm.c: 가상 메모리 객체를 위한 공통 인터페이스. */
#include <string.h>

#include "threads/mmu.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include "include/threads/vaddr.h"
#include "include/threads/thread.h"

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
static page_initializer get_page_initializer(enum vm_type type);


/* initializer와 함께 대기 중인(pending) 페이지 객체를 만든다.
 * 페이지를 만들고 싶다면 직접 만들지 말고 이 함수나
 * `vm_alloc_page`를 통해 만들 것. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	if (spt_find_page (spt, upage) == NULL) {
    struct page *page = malloc(sizeof(struct page));
    if (!page) PANIC("kernal malloc failed");

    page_initializer initializer = get_page_initializer(type);
    uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;

    if (!spt_insert_page(spt, page)){
      vm_dealloc_page(page);
      return false;
    }
    
    return true;
	}
  else{
    uninit_free_aux((struct aux *)aux);
  	return false;
  }  
}

static page_initializer
get_page_initializer(enum vm_type type){
  switch (VM_TYPE(type)){
  case VM_ANON:
    return anon_initializer;
  case VM_FILE:
    return file_backed_initializer;
  default:
    return NULL;
  }
}

/* spt에서 VA를 찾아 페이지를 돌려준다. 실패하면 NULL을 돌려준다. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	ASSERT(spt);

  struct page *page = NULL;
	struct page temp = {.va = pg_round_down(va)};

	struct hash_elem *e = hash_find(&spt->hash, &temp.hash_elem); 
	if (e != NULL)	  
    page = hash_entry(e, struct page, hash_elem);
	
  return page;
}

/* 검증을 거쳐 PAGE를 spt에 넣는다. 주소 유효성 검증은 vm_try_handle_fault에서 처리하는 것으로 구현 */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;

	struct hash_elem *old = hash_insert(&spt->hash, &page->hash_elem);
	if (old == NULL)
		succ = true;

  return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	ASSERT(spt && page);
	ASSERT(spt_find_page(spt, page->va) == page);

	hash_delete(&spt->hash, &page->hash_elem);
	vm_dealloc_page (page);
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
 * 메모리 공간을 만든다. 
 */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	void *kva = palloc_get_page(PAL_USER | PAL_ZERO);
	if (!kva)
		PANIC("todo"); // swap단계에서 vm_evict_frame으로 교체
	
  frame = malloc(sizeof(struct frame));
	ASSERT (frame != NULL);
  
	frame->kva = kva;
	frame->page = NULL;
	ASSERT (frame->page == NULL);
	return frame;
}

static void remove_pte_from_pml4(struct frame *frame, uint64_t *pml4);
static void disconnect_page(struct frame *frame);

/* frame->kva와 frame을 해제하고, page와의 연결을 끊는다.
  즉, frame과 page는 연결이 되어 있는 상태여야 한다.
  단, page의 메모리를 직접 해제하는 책임은 지지 않는다. */
void 
vm_remove_frame(struct frame *frame, uint64_t *pml4){
  ASSERT(frame);
  ASSERT(frame->page);

  // TODO : frame table에서 해당 frame 제거

  remove_pte_from_pml4(frame, pml4); 

  palloc_free_page(frame->kva);
  
  disconnect_page(frame);

  free(frame);
}

static void
remove_pte_from_pml4(struct frame *frame, uint64_t *pml4){
  struct page *page = frame->page;

  // remove frame 호출 과정에서, page->va에 해당하는 PTE가 가리키는 프레임이 frame과 불일치할 수 있음
  if (pml4_get_page(pml4, page->va) == frame->kva){
      pml4_clear_page(pml4, page->va);
  }
}

static void
disconnect_page(struct frame *frame){
  struct page *page = frame->page;
  // 페이지가 자신을 바라볼 때만 연결 끊기
  if (page->frame == frame){
    page->frame = NULL;
  }
}

/* 스택을 키운다. */
static void
vm_stack_growth (void *va) {
  vm_alloc_page(VM_ANON || VM_MARKER_0, va, true);
}

/* 쓰기 보호된(write-protected) 페이지에서 난 폴트를 처리한다. */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

static bool
is_stack_growth (struct intr_frame *f, void *addr, bool user) {
  uintptr_t rsp = user ? f->rsp : thread_current ()->user_stack_rsp;
  uintptr_t va = (uintptr_t) addr;

  /* 커널 모드는 PUSH가 발생하더라도 유저 스택 포인터(rsp)가 영향받지 않음 */
  uintptr_t rsp_valid = user ? rsp - 8 : rsp;

  return (va >= USER_STACK_END && va < USER_STACK && va >= rsp_valid);
}

/* Anonymous Page의 Lazy Loading, Stack Growth까지 해야 최종 구현 가능. 폴트 유효성 검사에 성공하면 true를 돌려준다. */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	ASSERT(spt);
	
	if (addr == NULL || !is_user_vaddr(addr))
		return false;

  if (!not_present)
		return false;

  void *page_addr = pg_round_down(addr); 
	page = spt_find_page(spt, page_addr);

  if (page == NULL) {
    if (!is_stack_growth(f, addr, user)) {
      return false;
    }

    vm_stack_growth(page_addr);
    page = spt_find_page(spt, page_addr);
    if (page == NULL) {
      return false;
    }
  }

  if (write && !page->writable)
		return false;

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
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = spt_find_page(spt, va);
	if (page == NULL) 
		return false;

  return vm_do_claim_page (page);
}

/* PAGE를 확보하고 MMU를 설정한다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
  frame->page = page;
	page->frame = frame;

  uint64_t *pml4 = thread_current()->pml4;
	if (!pml4_set_page(pml4, page->va, frame->kva, page->writable)) {
		vm_remove_frame(frame, pml4);
		return false;
	}

	return swap_in (page, frame->kva);
}

unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	ASSERT(spt);
	if (!hash_init(&spt->hash, page_hash, page_less, NULL))
		PANIC("SPT hash 초기화 실패");
}

static bool try_copy_uninit(struct page *page);
static bool try_copy_anon(struct page *page);
static bool try_copy_file(struct page *page);


bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
                             struct supplemental_page_table *src) {
  struct hash_iterator i;
  hash_first (&i, &src->hash);

  while (hash_next (&i)) {
    struct hash_elem *e = hash_cur (&i);
    struct page *page = hash_entry(e, struct page, hash_elem);

    enum vm_type type = VM_TYPE(page->operations->type);

    switch (type)
    {
    case VM_UNINIT:
      if (!try_copy_uninit(page)) return false;
      break; 
    case VM_ANON:
      if (!try_copy_anon(page)) return false;
      break;
    case VM_FILE:
      if (!try_copy_file(page)) return false;
      break;
    default:
      return false;
    }
  }
  return true;
}

static bool
try_copy_uninit(struct page *page){
  struct uninit_page *uninit = &page->uninit;
  struct aux * aux = uninit->aux;      
  struct aux * new_aux = aux->duplicate_func(aux);

  if (!vm_alloc_page_with_initializer(uninit->type, page->va, page->writable, uninit->init, new_aux)){
    return false;
  }

  if (!vm_claim_page(page->va)){
    return false;
  }

  return true;
}

static bool
try_copy_anon(struct page *page){
  if (!vm_alloc_page(page->operations->type, page->va, page->writable)) return false;
  if (!vm_claim_page(page->va)) return false;

  struct page *new_page = spt_find_page(&thread_current()->spt, page->va);
  memcpy(new_page->frame->kva, page->frame->kva, PGSIZE);

  return true;
}

static bool
try_copy_file(struct page *page){
  if (!vm_alloc_page(page->operations->type, page->va, page->writable)) return false;
  if (!vm_claim_page(page->va)) return false;

  return true;
}

/* 해시 요소에 해당하는 페이지를 정리한다. */
static void
page_destroy (struct hash_elem *e, void *aux UNUSED) {
    struct page *page = hash_entry(e, struct page, hash_elem);
    vm_dealloc_page(page);
}


/* 보조 페이지 테이블이 가진 자원을 해제한다 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	hash_destroy(&spt->hash, page_destroy);
}