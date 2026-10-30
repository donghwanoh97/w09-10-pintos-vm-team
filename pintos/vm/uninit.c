/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "userprog/process.h"
#include "vm/vm.h"
#include "vm/uninit.h"
#include "userprog/syscall.h"
#include <string.h>
#include "threads/malloc.h"
#include "filesys/file.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);
static void uninit_free_resources(void *aux);


/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;
  bool succ = uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
  // 이미 page_initializer로 다른 타입으로 바뀐 후임 -> uninit 접근 불가
  uninit_free_resources(aux);
  return succ;
}

bool
lazy_load_segment (struct page *page, void *aux) {
	struct lazy_load_args *args = (struct lazy_load_args *)((struct aux *) aux)->aux_pt;
  void *kva = page->frame->kva;
  lock_acquire(&filesys_lock);
  bool succ = file_read_at (args->file, kva, args->read_bytes, args->ofs) == (off_t)args->read_bytes;
  lock_release(&filesys_lock);
  if (succ) memset (kva + args->read_bytes, 0, args->zero_bytes);
  return succ;
}

/* load_segment, do_mmap 공통. 해제는 uninit 몫 */
struct aux *
make_lazy_load_aux(struct file *file, off_t ofs, size_t read_bytes, size_t zero_bytes, duplicate_aux dup) {
	struct aux *aux = malloc(sizeof *aux);
	struct lazy_load_args *args = malloc(sizeof *args);
	if (!aux || !args)
		PANIC("kernel malloc failed");	
	args->file = file;
  args->ofs = ofs;
  args->read_bytes = read_bytes;
  args->zero_bytes = zero_bytes;  
	aux->aux_pt = args;
  aux->duplicate_func = dup; /* fork 때 쓸 복사 함수 */
	return aux;
}
/* fork 복사 공통. 부모 aux 값은 그대로. file만 자식 소유로 바꿔서 새 aux를 만든다. */
struct aux *
copy_lazy_load_aux(void * aux, struct file *file){
  struct aux *src = aux;
	struct aux *dst = malloc(sizeof *dst);
	struct lazy_load_args *new_arg = malloc(sizeof *new_arg);
  if (!new_arg || !dst)
    PANIC("kernel malloc failed.");  
	*new_arg = *(struct lazy_load_args *)src->aux_pt;
	new_arg->file = file;
	*dst = *src;
	dst->aux_pt = new_arg;
	return dst;
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit = &page->uninit;
	/* mmap 페이지면 reopen한 file의 주인이 이 페이지 → 닫기.
	   실행 파일 페이지(VM_ANON)는 using_file을 빌려 쓴 것 → 닫지 않음 */
	if (uninit->aux != NULL && VM_TYPE (uninit->type) == VM_FILE) {
		struct lazy_load_args *args = ((struct aux *) uninit->aux)->aux_pt;
		lock_acquire (&filesys_lock);
		file_close (args->file);
		lock_release (&filesys_lock);
	}
  uninit_free_resources(uninit->aux);
  uninit->aux = NULL;
}

static void
uninit_free_resources(void *aux){
  // 다른 리소스 있을 경우 여기 추가
  uninit_free_aux((struct aux *) aux);
}

void
uninit_free_aux(struct aux *aux){
  if (aux == NULL) return;

  ASSERT(aux->aux_pt);
  free(aux->aux_pt);
  free(aux);
}