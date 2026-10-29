---
title: [3-2] 첫 스택 페이지 (setup_stack)
milestone: Anonymous Page: Lazy Loading
labels: type:feature, area:anon
tests: args-none, args-single, args-multiple, args-many, args-dbl-space, halt, exit, create-normal, create-empty, create-null, create-bad-ptr, create-long, create-exists, create-bound, open-normal, open-missing, open-boundary, open-empty, open-null, open-bad-ptr, open-twice, close-normal, close-twice, close-bad-fd, read-normal, read-bad-ptr, read-boundary, read-zero, read-stdout, read-bad-fd, write-normal, write-bad-ptr, write-boundary, write-zero, write-stdin, write-bad-fd, exec-once, exec-arg, exec-missing, exec-bad-ptr, wait-bad-pid, rox-simple
---
## 목표
스택 첫 페이지를 할당하고 즉시 claim해서 유저 프로그램이 실행되게 한다. 이게 안 되면 모든 유저 프로그램이 `Fail to launch initd`로 PANIC한다.

## 문서
- [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html)

## 할 일
- [ ] `vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom, true)` + `vm_claim_page(stack_bottom)`
- [ ] `if_->rsp = USER_STACK;`
- [ ] 성공 시 `success = true` 반환
- [ ] 주소 전달용으로 따로 malloc한 `struct page` 제거 (SPT에 들어가지 않고 누수됨)

## 참고
- exec-once, exec-arg, exec-missing, exec-bad-ptr, wait-bad-pid, rox-simple은 일정표상 4번(fork)에 있었지만 fork 없이 이 단계에서 통과하므로 여기로 옮겼다.

<!-- TESTS -->
