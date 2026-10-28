/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include "threads/vaddr.h"
#include "include/userprog/syscall.h"
#include "threads/mmu.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
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

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *page = NULL;
	ASSERT(spt != NULL);

	struct page temp_page = {.va = pg_round_down(va) };
	
	struct hash_elem *found = hash_find(&spt->hash, &temp_page.hash_elem);

	if (found != NULL) {
		page = hash_entry(found, struct page, hash_elem);
	}

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	ASSERT(spt != NULL && page != NULL);

	int succ = false;

	if (hash_insert(&spt->hash, &page->hash_elem) == NULL) {
		succ = true;
	}
	
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	ASSERT(spt != NULL && page != NULL);

	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	frame = malloc(sizeof frame);
	if(!frame) {
		sys_exit(-1);
	}

	uint64_t kva = palloc_get_page(PAL_USER | PAL_ZERO);
	if (!kva) { 
		// swap out
		PANIC("todo");

		// if swap full, sys_exit(-1)
	}

	frame->kva = kva;
	frame->page = NULL;

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
// todo - setup_stack 또는 vm_stack_growth에서 전파된 에러 처리
bool
vm_claim_page (void *va) {
	struct page *page = NULL;

	// create page and insert it into spt
	if (!vm_alloc_page(VM_UNINIT, pg_round_down(va), true)) {
		return false;
	}; 

	page = spt_find_page(thread_current()->spt, va);

	if (!page) {
		return false; 
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (!pml4_set_page(thread_current()->pml4, frame->page->va, frame->kva, frame->page->writable)) {
		return false;
	}
	
	return swap_in (page, frame->kva);
}

// 왼쪽 값이 오른쪽 값보다 작은지 판별
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);
	
  return a->va < b->va;
}

unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	// elem을 사용해서 struct page에 접근
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  // va 필드의 주소와 사이즈를 전달해서 해싱 
  return hash_bytes (&p->va, sizeof p->va);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	ASSERT(spt != NULL);
	
	if (!hash_init(&spt->hash, page_hash, page_less, NULL)) {
		sys_exit(-1);
	}
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

static void page_destroy(struct hash_elem *elem, void* aux UNUSED) {
	struct page *p = hash_entry(elem, struct page, hash_elem); 
	vm_dealloc_page(p);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	ASSERT(spt != NULL);

	// hash_destroy 호출 시점 고민
	hash_clear(&spt->hash, page_destroy);
}
