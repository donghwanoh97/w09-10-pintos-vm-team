#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"
#include "filesys/off_t.h"

struct file;
struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

bool read_file_into_frame (struct file *file, off_t ofs, size_t read_bytes, void *kva);
bool lazy_load_segment (struct page *page, void *aux);

/* Uninitlialized page. The type for implementing the "Lazy loading". */
struct uninit_page {
	/* Initiate the contets of the page */
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	/* Initiate the struct page and maps the pa to the va */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

/* lazy load 때 파일의 어디를 얼마나 읽을 지 (로드 후에는 Free) */
struct lazy_load_info {
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};

typedef void *(*duplicate_aux)(void *aux);
struct aux {
  void *aux_pt; // aux_pt: lazy_load_args, mmap_args 등
  duplicate_aux duplicate_func; // aux를 복사할 때 호출되는 함수 포인터
};
struct aux *create_lazy_load_aux (struct file *file, off_t ofs, size_t read_bytes,
                                size_t zero_bytes, duplicate_aux dup);
struct aux *duplicate_lazy_load_aux (void *aux, struct file *file); 

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
void uninit_free_aux(struct aux *aux);
#endif
