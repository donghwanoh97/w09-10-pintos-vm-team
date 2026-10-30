#ifndef THREADS_VADDR_H
#define THREADS_VADDR_H

#include <debug.h>
#include <stdint.h>
#include <stdbool.h>

#include "threads/loader.h"

/* 가상 주소(virtual address)를 다루는 함수와 매크로.
 *
 * x86 하드웨어 페이지 테이블(page table)에 특화된 함수와 매크로는
 * pte.h를 참고할 것. */

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* 페이지 오프셋(page offset) (비트 0:12). */
#define PGSHIFT 0                          /* 첫 번째 오프셋 비트의 인덱스. */
#define PGBITS  12                         /* 오프셋 비트 수. */
#define PGSIZE  (1 << PGBITS)              /* 페이지 하나의 바이트 수. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* 페이지 오프셋 비트 (0:12). */

/* 페이지 내 오프셋. */
#define pg_ofs(va) ((uint64_t) (va) & PGMASK)

#define pg_no(va) ((uint64_t) (va) >> PGBITS)

/* 가장 가까운 페이지 경계로 올림함. */
#define pg_round_up(va) ((void *) (((uint64_t) (va) + PGSIZE - 1) & ~PGMASK))

/* 가장 가까운 페이지 경계로 내림함. */
#define pg_round_down(va) (void *) ((uint64_t) (va) & ~PGMASK)

/* 커널 가상 주소(kernel virtual address) 시작 */
#define KERN_BASE LOADER_KERN_BASE

/* 유저 스택(user stack) 시작 */
#define USER_STACK 0x47480000

/* 유저 스택(user stack) 끝 */
#define USER_STACK_END USER_STACK - 0x100000

/* VADDR가 유저 가상 주소(user virtual address)이면 true를 반환함. */
#define is_user_vaddr(vaddr) (!is_kernel_vaddr((vaddr)))

/* VADDR가 커널 가상 주소(kernel virtual address)이면 true를 반환함. */
#define is_kernel_vaddr(vaddr) ((uint64_t)(vaddr) >= KERN_BASE)

// FIXME: 검사 추가할 것
/* 물리 주소(physical address) PADDR가 매핑된
 *  커널 가상 주소를 반환함. */
#define ptov(paddr) ((void *) (((uint64_t) paddr) + KERN_BASE))

/* 커널 가상 주소 VADDR가 매핑된
 * 물리 주소를 반환함. */
#define vtop(vaddr) \
({ \
	ASSERT(is_kernel_vaddr(vaddr)); \
	((uint64_t) (vaddr) - (uint64_t) KERN_BASE);\
})

#endif /* threads/vaddr.h */
