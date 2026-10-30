#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
  bool is_stack;
  size_t swap_slot; /* 내보낸 칸 번호 */
};

#define IS_STACK(type) ((type) & VM_MARKER_0)

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
