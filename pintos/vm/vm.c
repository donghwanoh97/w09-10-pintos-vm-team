/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"

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
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
  ASSERT(spt && va);
	if (va >= KERN_BASE) return NULL;

  struct page *page = NULL;
  struct page temp = {.va = va};

  struct hash_elem *found = hash_find(&spt->hash, &temp.elem);
  if (found)
    page = hash_entry(found, struct page, elem);
	
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
  ASSERT(spt && page);
  int succ = false;
	
	struct hash_elem *old = hash_insert(&spt->hash, &page->elem);	
	int succ = (old == NULL);

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
  ASSERT(spt && page);
	ASSERT(spt_find_page(spt, page->va) == page);

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
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

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

	return swap_in (page, frame->kva);
}

static uint64_t page_hash_func (const struct hash_elem *e, void *aux);
static bool page_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void page_destroy(const struct hash_elem *e, void *aux);

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
  ASSERT(spt);
  
  int succ = hash_init(&spt->hash, page_hash_func, page_less_func, NULL);

  if (!succ){
    sys_exit(-1);
  }
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
  ASSERT(dst && src);

  struct hash_iterator i;
  hash_first(&i, &src->hash);
  while(hash_next(&i)){
    const struct page *p_src = hash_entry(hash_cur(&i), struct page, elem);

    struct page *p_dst = malloc(sizeof(struct page));
    if (p_dst == NULL){
      goto fail_malloc;
    }

    struct frame *frame = NULL;
    enum vm_type type = p_src->operations->type;
    bool rw = false; // TODO : rw 권한 추가하기
    p_dst->operations = p_src->operations;
    p_dst->va = p_src->va;

    switch (type)
    {
    case VM_UNINIT:
    {
      const struct uninit_page *uninit = &p_src->uninit;
      uninit_new(p_dst, p_src->va, uninit->init, uninit->type, uninit->aux, uninit->page_initializer);
    }
      break;
    case VM_ANON:
    {
      frame = vm_get_frame ();
      p_dst->frame = frame;
      frame->page = p_dst;

      if (!anon_initializer(p_dst, type, frame->kva)){
        goto fail_page; 
      }
      // TODO: 프레임 복제
      
      if (!pml4_set_page(thread_current()->pml4, p_dst->va, frame->kva, rw)){
        goto fail_pt;
      }
    }
      break;
    case VM_FILE:
    {
      frame = vm_get_frame ();
      p_dst->frame = frame;
      frame->page = p_dst;

      if (!file_backed_initializer(p_dst, type, frame->kva)){
        goto fail_page;
      }
      // TODO : 프레임 복제

      if (!pml4_set_page(thread_current()->pml4, p_dst->va, frame->kva, rw)) {
        goto fail_pt;
      }
    }
      break;
    default:
      goto fail_page;
    }

    if (spt_insert_page(dst, p_dst)){
      continue;
    }

fail_pt:
    destroy(p_dst);
fail_page:
    free(p_dst);
    // TODO : 프레임 NULL이 아니라면 해제
fail_malloc:
    return false;
  }
  return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
  ASSERT(spt);

  hash_destroy(&spt->hash, page_destroy);
}

static uint64_t
page_hash_func (const struct hash_elem *e, void *aux){
  const struct page *p = hash_entry(e, struct page, elem);
  return hash_bytes(&p->va, sizeof p->va);
}

static bool 
page_less_func(const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux){
  const struct page *pa = hash_entry(a, struct page, elem);
  const struct page *pb = hash_entry(b, struct page, elem);

  return pa->va < pb->va;
}

static void 
page_destroy(const struct hash_elem *e, void *aux){
  const struct page *p = hash_entry(e, struct page, elem);
  
  vm_dealloc_page(p);
}
