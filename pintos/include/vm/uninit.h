#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* 초기화되지 않은 페이지. "Lazy loading"을 구현하기 위한 타입. */
struct uninit_page {
	vm_initializer *init;   /* 페이지 내용을 채우는 함수 */
	enum vm_type type;      /* 나중에 될 타입 */
	void *aux;              /* init에 넘길 정보 */
	/* struct page를 초기화하고 pa를 va에 매핑하는 함수 */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};
/* 페이지를 새로 만들어서 반환 */
void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
