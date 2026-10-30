#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"
#include "filesys/off_t.h"

struct file;
struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* Uninitlialized page. The type for implementing the "Lazy loading". */
struct uninit_page {
	/* Initiate the contets of the page */
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	/* Initiate the struct page and maps the pa to the va */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

typedef void *(*duplicate_aux)(void *aux);
struct aux {
  void *aux_pt;
  duplicate_aux duplicate_func;
};
struct aux *make_lazy_load_aux (struct file *file, off_t ofs, size_t read_bytes,
                                size_t zero_bytes, duplicate_aux dup);
struct aux *copy_lazy_load_aux (void *aux, struct file *file);

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
void uninit_free_aux(struct aux *aux);

#endif
