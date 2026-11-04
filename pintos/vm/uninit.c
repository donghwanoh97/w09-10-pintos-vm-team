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
#include "include/threads/vaddr.h"
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
	.type = VM_UNINIT, };

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);
	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL,   /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init, 
			.type = type, 
			.aux = aux,
			.page_initializer = initializer, 
		} 
	}; 
}

/* Initalize the page on first fault */
/* 첫 page fault 때 UNINIT 페이지를 실제 페이지로 만든다.
   1) page_initializer: 타입을 ANON/FILE로 바꾼다.
   2) init (지연 로드 콜백): 페이지를 만들 때 맡겨 둔 함수로 프레임(kva)을 채운다.
      lazy_load_segment(실행 파일) / lazy_load_mmap(mmap). 스택처럼 init이 없으면 생략.
   3) aux는 다 썼으므로 해제한다. */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;
	vm_initializer *init = uninit->init; 
	void *aux = uninit->aux;  
  bool succ = uninit->page_initializer (page, uninit->type, kva) &&  
		(init ? init (page, aux) : true);
	uninit_free_resources(aux); 
		return succ;
 }

/* 지연 로드 콜백 (uninit_initialize가 첫 fault 때 init으로 부름).
   aux에 적힌 file·ofs·read_bytes대로 파일을 읽어 프레임을 채운다. 실행 파일·mmap 공통. */
bool
lazy_load_segment (struct page *page, void *aux) {
	struct lazy_load_info *lazy = ((struct aux *) aux)->aux_pt;
	return read_file_into_frame(lazy->file, lazy->ofs, lazy->read_bytes, page->frame->kva); 
}

/* file의 ofs부터 read_bytes를 frame(kva)에 읽고, 나머지는 0으로 채운다. */
bool
read_file_into_frame (struct file *file, off_t ofs, size_t read_bytes, void *kva) {
  lock_acquire(&filesys_lock);
  bool succ = file_read_at (file, kva, read_bytes, ofs) == (off_t)read_bytes; 
	lock_release(&filesys_lock);
  if (succ) memset (kva + read_bytes, 0, PGSIZE - read_bytes); return succ; 
}

/* 지연 로드에 쓸 정보(파일, 위치, 크기)를 담은 aux를 새로 만든다 (load_segment, do_mmap에서 사용). 
	dup은 복제 방법으로 저장만 한다. */
struct aux *
create_lazy_load_aux(struct file *file, off_t ofs, size_t read_bytes, size_t zero_bytes, duplicate_aux dup) {
	struct aux *aux = malloc(sizeof *aux);
	struct lazy_load_info *lazy = malloc(sizeof *lazy);
	if (!aux || !lazy) PANIC("kernel malloc failed");	
	lazy->file = file; 
	lazy->ofs = ofs; 
	lazy->read_bytes = read_bytes;
  lazy->zero_bytes = zero_bytes; 
	aux->aux_pt = lazy;
  aux->duplicate_func = dup; 
	return aux; 
}

/* aux를 복사한다. 파일만 인자로 받은 것으로 바꾼다 */
struct aux *
duplicate_lazy_load_aux(void * aux, struct file *file) {
  struct aux *src = aux; 
	struct aux *dst = malloc(sizeof *dst);
	struct lazy_load_info *lazy = malloc(sizeof *lazy);
  if (!lazy || !dst) PANIC("kernel malloc failed.");  
	*lazy = *(struct lazy_load_info *)src->aux_pt; 
	lazy->file = file; 
	*dst = *src; 
	dst->aux_pt = lazy; 
	return dst; 
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit = &page->uninit;
	if (is_mmap_page(page) && uninit->aux != NULL) {
		struct lazy_load_info *lazy = ((struct aux *) uninit->aux)->aux_pt;
		close_backing_file(lazy->file); 
	}
  uninit_free_resources(uninit->aux); 
	uninit->aux = NULL; 
}

/* uninit 페이지가 가진 자원(aux)을 해제한다 - 다른 리소스 있을 경우 여기 추가 */
static void
uninit_free_resources(void *aux) { 
	uninit_free_aux((struct aux *) aux); 
}

/* aux와 args 구조체 메모리만 free한다. args->file은 닫지 않는다 */
void
uninit_free_aux(struct aux *aux){
  if (aux == NULL) return; 
	ASSERT(aux->aux_pt); 
	free(aux->aux_pt); 
	free(aux); 
}