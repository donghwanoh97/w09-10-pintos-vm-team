/* vm.c: 가상 메모리 객체를 위한 공통 인터페이스. */
#include <string.h>
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/vm.h"
#include "vm/file.h"
#include "vm/inspect.h"
#define PUSH_SIZE 8   /* PUSH 명령이 rsp보다 8바이트 아래를 먼저 건드림 */

/* 현재 스레드의 SPT (fork 중에는 자식) */
static struct supplemental_page_table * cur_spt(void) { return &thread_current()->spt; }
static uint64_t page_hash (const struct hash_elem *e, void *aux);
static bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
/* 도우미 함수들 */
static page_initializer get_page_initializer(enum vm_type type);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_get_victim (void);
static struct frame *vm_evict_frame (void);
/* 스택 성장 */
static uintptr_t cur_user_rsp (struct intr_frame *f, bool user);
static uintptr_t lowest_stack_addr (uintptr_t rsp, bool user);
static bool is_stack_range (void *addr, uintptr_t rsp, bool user);
static void vm_stack_growth (void *addr);
/* 프레임 구조체 해제 */
static void free_frame (struct frame *frame);
/* VM_TYPE별 COPY */
static bool copy_uninit_page(struct page *parent_page);
static bool copy_anon_page(struct page *parent_page);
static bool copy_file_page(struct page *parent_page);
static bool copy_info_and_reopen_file(struct page *parent_page, struct page *child_page);
static struct aux *copy_aux(struct aux *aux);
static bool copy_frame_content (struct page *parent_page, struct page *child_page);
static struct page *claim_child_page (struct page *parent_page);
/* Frame Table */
static struct list *frame_table;
static struct list_elem *clock_hand;
static struct lock frame_table_lock;


/* ───── 초기화 ───── */ 
void
vm_init (void) {
	vm_anon_init (); vm_file_init ();
  lock_init(&frame_table_lock);
#ifdef EFILESYS  /* Project 4용 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* 위의 줄들은 수정하지 말 것. */
	/* TODO: 여기에 코드를 작성. */ 
}

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

/* upage에 페이지를 등록한다. 지금은 UNINIT이고, 첫 page fault 때 type(ANON/FILE)으로 바뀐다.
   이미 있거나 실패하면 false (aux는 해제) */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
                                vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE (type) != VM_UNINIT);
	struct supplemental_page_table *spt = cur_spt ();
	if (spt_find_page (spt, upage) != NULL) {
		uninit_free_aux (aux);
		return false;
	}
	struct page *page = malloc (sizeof (struct page));
	if (page == NULL)
		PANIC ("kernel malloc failed");
	uninit_new (page, upage, init, type, aux, get_page_initializer (type));
	page->writable = writable;
	if (!spt_insert_page (spt, page)) {
		vm_dealloc_page (page);
		return false;
	}
	return true;
}

static page_initializer
get_page_initializer(enum vm_type type) {
  switch (VM_TYPE(type)) { 
    case VM_ANON: 
      return anon_initializer;
    case VM_FILE: 
      return file_backed_initializer; 
    default: 
      return NULL; 
    } 
}

/* ───── SPT ───── */ 
/* spt에서 VA를 찾아 페이지를 돌려준다. 실패하면 NULL을 돌려준다. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	ASSERT(spt);
  struct page *page = NULL; 
  struct page temp = {.va = pg_round_down(va)};
	struct hash_elem *e = hash_find(&spt->hash, &temp.hash_elem); 
	if (e != NULL) 
    page = hash_entry(e, struct page, hash_elem);
  return page; }

/* SPT 해시에서 page->va에 해당하는 페이지를 제거한다. */
void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	ASSERT(spt && page);
	ASSERT(spt_find_page(spt, page->va) == page);
	hash_delete(&spt->hash, &page->hash_elem); }

/* 검증을 거쳐 PAGE를 spt에 넣는다. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;
	struct hash_elem *old = hash_insert(&spt->hash, &page->hash_elem);
	if (old == NULL) succ = true; return succ; 
}

void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	ASSERT(spt);
  if (!hash_init(&spt->hash, page_hash, page_less, NULL))
   PANIC("SPT hash 초기화 실패"); 
}
    
static uint64_t
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

/* ───── 프레임 ───── */  
/* palloc()으로 frame을 얻는다. 사용 가능한 페이지가 없으면 페이지를 내보내고 그 frame을 돌려준다. 이 함수는 항상 유효한 주소를 돌려준다. 즉, user pool 메모리가 가득 차면 frame을 내보내서 사용 가능한 메모리 공간을 만든다. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	void *kva = palloc_get_page(PAL_USER | PAL_ZERO);
	frame = (kva == NULL) ? vm_evict_frame() : create_frame(kva);
  return frame;
}

static struct frame* create_frame(void *kva) {
	struct frame *frame = NULL;
  frame = malloc(sizeof(struct frame));
  if (frame == NULL) PANIC("malloc failed");
	ASSERT (frame != NULL); 
  frame->kva = kva; 
  frame->page = NULL;
	ASSERT (frame->page == NULL); 
  return frame; 
}

/* 내보낼(evict) struct frame을 고른다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;

  lock_acquire (&frame_table_lock);

  if (list_empty(&frame_table)) {
    lock_release(&frame_table_lock);
    return NULL;
  }

  struct list_elem *curr = clock_hand;
  if (curr == NULL || curr == list_end(&frame_table)) {
    curr = list_begin(&frame_table);
  }
  
  while (true) {
    struct frame *f = list_entry(curr, struct frame, elem);
    if (!pml4_is_accessed(f->page->pml4, f->page->va)) {
      clock_hand = list_remove(curr);

      if (clock_hand == list_end(&frame_table)) {
        clock_hand = list_empty(&frame_table) ? NULL : list_begin(&frame_table);
      }
      f->elem.prev = NULL;
      f->elem.next = NULL;
      victim = f;
      break;
    } 

    pml4_set_accessed(f->page->pml4, f->page->va, false);

    curr = list_next(curr);

    if (curr == list_end(&frame_table)) {
      curr = list_begin(&frame_table);
    }
  }
  
  lock_release(&frame_table_lock);
	return victim; 
}

/* 페이지 하나를 내보내고 그 frame을 돌려준다. 실패하면 NULL을 돌려준다. */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();

  if(!victim) {
    return NULL;
  }

  if (!swap_out(victim->page)) {
    lock_acquire(&frame_table_lock);
    list_push_back(&frame_table, &victim->elem);
    lock_release(&frame_table_lock);
    return NULL;
  }

  pml4_clear_page(victim->page->pml4, victim->page->va);
  victim->page->frame = NULL;
  victim->page = NULL;

  memset(victim->kva, 0, PGSIZE);

	return victim; 
}

/* 1) pml4에서 page->va 해당 칸 PTE를 지우고, 2) page와 frame의 연결을 해제, 3) frame->kva(물리 메모리)와 frame 구조체를 반납한다. */
void 
release_frame(struct frame *frame, uint64_t *pml4){
  ASSERT(frame); 
  ASSERT(frame->page); 
  remove_pte_from_pml4(frame, pml4); 
  unlink_page_and_frame(frame);

  free_frame(frame); 
}

/* pml4에서 page->va가 이 frame을 가리킬 때만, 이 칸 PTE를 지운다. */
void
remove_pte_from_pml4(struct frame *frame, uint64_t *pml4) {
  struct page *page = frame->page;
  if (pml4_get_page(pml4, page->va) == frame->kva) { 
    pml4_clear_page(pml4, page->va); 
  } 
}

/* page와 frame 연결을 해제한다. */
void
unlink_page_and_frame(struct frame *frame){
  struct page *page = frame->page;
  if (page->frame == frame) { 
    page->frame = NULL; 
    frame->page = NULL; 
  } 
}

/* frame->kva(물리 메모리)와 frame 구조체를 반납한다. */
static void
free_frame (struct frame *frame) { 
  palloc_free_page (frame->kva); 
  free(frame); 
}

/* ───── 폴트 · 스택 성장 ───── */ 
/* 페이지 폴트: 1) 유효한 접근인가? (사용자 주소, present 아님, 권한)
            2) SPT에 등록된 페이지가 있는지? 없으면 스택 성장인가? 
            3) vm_do_claim_page: 프레임 받기 + PTE 연결 + 내용 채우기 
            이후 true를 반환하게 되면 이번엔 CPU가 PTE가 있으니 성공 */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	if (!is_user_vaddr(addr) || !not_present) return false;
  struct page *page = spt_find_page(cur_spt(), addr);
  if (page == NULL) {
    if (!is_stack(addr, cur_user_rsp(f, user), user))
      return false; 
    vm_stack_growth(addr);
    page = spt_find_page(cur_spt(), addr);
  }
  if (page == NULL || (write && !page->writable)) return false;
  return vm_do_claim_page (page); 
}

bool
is_stack (void *addr, uintptr_t rsp, bool user) {
	return STACK_LIMIT <= rsp 
  && rsp <= USER_STACK 
  && is_stack_range(addr, rsp, user); 
}

static bool
is_stack_range(void *addr, uintptr_t rsp, bool user) {
  uintptr_t a = (uintptr_t) addr;
  return STACK_LIMIT <= a
   && a < USER_STACK 
   && lowest_stack_addr(rsp, user) <= a;
}

static uintptr_t
lowest_stack_addr (uintptr_t rsp, bool user) { 
  return user ? rsp - PUSH_SIZE : rsp; 
}

static uintptr_t
cur_user_rsp (struct intr_frame *f, bool user) {
  uintptr_t rsp = user ? f->rsp : thread_current()->user_rsp; 
  return rsp; 
}

static void
vm_stack_growth (void *addr) {
  vm_alloc_page(VM_ANON | VM_MARKER_0, pg_round_down(addr), true); 
}

/* 쓰기 보호된(write-protected) 페이지에서 난 폴트를 처리한다. */
static bool
vm_handle_wp (struct page *page UNUSED) {	
  return false;   
  /* COW 할 때 구현 */ 
}

/* ───── claim ───── */ 
/* VA에 할당된 페이지를 확보(claim)한다. */
bool
vm_claim_page (void *va) {
	struct page *page = spt_find_page(cur_spt(), va); 
  if (page == NULL) return false;
  return vm_do_claim_page (page); 
}

/* 프레임을 받아 page와 연결하고, pml4에 va->kva를 등록한 뒤, swap_in으로 내용을 채운다.
(UNINIT이면 파일에서 읽어 첫 로드, 쫓겨났던 페이지면 ANON의 경우 swap 디스크, FILE의 경우 
파일에서 다시 읽어 옴) */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
  if(!frame) {
    return false;
  }

  frame->page = page;	page->frame = frame; 
  uint64_t *pml4 = page->pml4;

	if (!pml4_set_page(pml4, page->va, frame->kva, page->writable)) {
	release_frame(frame, pml4); return false; 
  } 

  if (!swap_in (page, frame->kva)) {
    release_frame(frame, pml4); 
    return false;
  }
  
  lock_acquire(&frame_table_lock);
  list_push_back(&frame_table, &frame->elem);
  lock_release(&frame_table_lock);
  
  return true;
}

/* 페이지를 예약하고 바로 frame을 붙임 - lazy 하지 않음 */
bool
vm_alloc_and_claim (enum vm_type type, void *va, bool writable) {
	return vm_alloc_page(type, va, writable) && vm_claim_page(va); 
}

/* ───── VM_TYPE별 SPT fork  ───── */  
bool
supplemental_page_table_copy (struct supplemental_page_table *dst, 
                              struct supplemental_page_table *src) {
  struct hash_iterator i; hash_first (&i, &src->hash);
  while (hash_next (&i)) {
    struct hash_elem *e = hash_cur (&i);
    struct page *page = hash_entry(e, struct page, hash_elem);
    switch (VM_TYPE(page->operations->type)) { 
      case VM_UNINIT: if (!copy_uninit_page(page)) return false; break; 
      case VM_ANON: if (!copy_anon_page(page)) return false; break;
      case VM_FILE: if (!copy_file_page(page)) return false; break;
      default: return false; 
    } 
    } return true; 
  }

/* aux의 사본을 만든다. 복사 방법은 aux를 만들 때 넣은 duplicate_func가 정한다.
      실행 파일: duplicate_segment_aux (process.c) / mmap: duplicate_mmap_aux (file.c) */
static struct aux *
copy_aux(struct aux *aux){ 
  if (aux == NULL) return NULL; 
  ASSERT(aux->duplicate_func); 
  return aux->duplicate_func(aux); 
}

/* 부모 uninit 페이지를 자식에게 copy한다. aux 사본으로 UNINIT 등록 (로드는 나중에) */
static bool
copy_uninit_page(struct page *page) {
  struct uninit_page *uninit = &page->uninit;
  struct aux * aux = uninit->aux; 
  struct aux * new_aux = copy_aux(aux);
  if (aux != NULL && new_aux == NULL) return false;
  return vm_alloc_page_with_initializer(uninit->type, page->va, page->writable, uninit->init, new_aux); 
}

/* 부모 프레임의 내용을 자식 프레임으로 복사한다. */
static bool
copy_frame_content(struct page *parent_page, struct page *child_page) {
  if (parent_page->frame == NULL) return false;
  memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE); 
  return true; 
}

/* 부모와 같은 자식 페이지를 만들고 프레임까지 받아서 돌려준다 (자식 SPT에 등록) */
static struct page *
claim_child_page(struct page *parent_page) {
  if (!vm_alloc_and_claim(parent_page->operations->type, parent_page->va, parent_page->writable))
    return NULL;
  return spt_find_page(cur_spt(), parent_page->va);
}

/* 부모 anon 페이지를 자식에게 복사: 새 프레임을 받고 내용 memcpy */
static bool
copy_anon_page(struct page *parent_page) {
  struct page *child_page = claim_child_page(parent_page);
  if (child_page == NULL) return false;
  if (!copy_frame_content(parent_page, child_page)) return false; 
  return true;
}

/* 부모 file 페이지를 자식에게 복사: info 복사(파일은 다시 씀) +  내용 memcpy */
static bool
copy_file_page(struct page *parent_page) {
  struct page *child_page = claim_child_page(parent_page);
  return child_page != NULL
  && copy_info_and_reopen_file(parent_page, child_page)
  && copy_frame_content(parent_page, child_page);
}

/* file page info를 copy한다. backing_file은 새로 연다 */
static bool
copy_info_and_reopen_file(struct page *parent_page, struct page *child_page) {
  struct file_page *parent_info = &parent_page->file;
  struct file_page *child_info = &child_page->file;
  child_info->offset = parent_info->offset;
  child_info->read_bytes = parent_info->read_bytes;
  child_info->backing_file = make_backing_file(parent_info->backing_file);
  if (child_info->backing_file == NULL) return false; 
  return true; 
}

/* ───── 해제 ───── */    
/* 페이지를 해제한다.
 * 이 함수는 수정하지 말 것. */
void
vm_dealloc_page (struct page *page) {
	destroy (page); free (page);
}

/* SPT 해시 요소에 해당하는 페이지를 정리한다. */
static void
spt_page_destroy (struct hash_elem *e, void *aux UNUSED) {
    struct page *page = hash_entry(e, struct page, hash_elem); 
    vm_dealloc_page(page); 
}

/* 보조 페이지 테이블이 가진 자원을 해제한다 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	hash_destroy(&spt->hash, spt_page_destroy); 
}