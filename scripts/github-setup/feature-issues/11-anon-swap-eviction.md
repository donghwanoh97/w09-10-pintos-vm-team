---
title: [7-1] anon swap + eviction
milestone: Swapping
labels: type:feature, area:swap, area:frame
tests: swap-anon, swap-iter
---
## 목표
물리 메모리가 부족하면 frame을 골라 내보내고(eviction), anon 페이지는 swap 디스크에 저장했다가 다시 읽어온다.

## 문서
- [Swap In/Out](https://casys-kaist.github.io/pintos-kaist/project3/swapping.html)
- [Memory Management › Frame Management](https://casys-kaist.github.io/pintos-kaist/project3/vm_management.html)

## 할 일
- [ ] `vm_anon_init`: swap 디스크(`disk_get(1, 1)`), 슬롯 테이블(bitmap)
- [ ] `struct anon_page`에 슬롯 번호 필드
- [ ] `anon_swap_out` → `anon_swap_in` (슬롯 해제 포함)
- [ ] frame table (`list_elem`, lock)
- [ ] `vm_get_victim` (clock, accessed bit), `vm_evict_frame`
- [ ] `vm_get_frame`의 PANIC을 eviction으로 교체
- [ ] `anon_destroy`에서 swap 슬롯 해제

## 회귀 확인
- [ ] page-linear, page-shuffle, page-parallel, page-merge-seq, page-merge-par가 swap 구현 후에도 통과 (이미 3-1, 4에서 닫힌 이슈)

<!-- TESTS -->
