---
title: [7-2] file swap + 복합 상황
milestone: Swapping
labels: type:feature, area:swap, area:mmap
tests: swap-file, swap-fork
---
## 목표
file-backed 페이지도 eviction 대상이 되게 하고, swap이 fork·mmap과 섞여도 동작하게 한다.

## 문서
- [Swap In/Out](https://casys-kaist.github.io/pintos-kaist/project3/swapping.html)

## 할 일
- [ ] `file_backed_swap_out`: dirty면 파일에 write-back 후 dirty 비트 초기화
- [ ] `file_backed_swap_in`: 파일에서 다시 읽기
- [ ] fork 중 부모 페이지가 swap out 상태일 때 복사 처리 (swap-fork)

## 회귀 확인
- [ ] page-merge-stk, page-merge-mm이 swap 구현 후에도 통과 (5, 6-1에서 닫힌 이슈)

## 참고
- swap-fork는 메모리 상황에 따라 swap 없이도 통과할 수 있다. 이 단계 PR이 머지될 때 닫는다.

<!-- TESTS -->
