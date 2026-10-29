---
title: [3-4] exit·exec 정리 (Page Cleanup)
milestone: Anonymous Page: Lazy Loading
labels: type:feature, area:anon
tests:
---
## 목표
프로세스 종료와 exec 때 페이지 관련 자원을 남김없이 해제한다. 전용 테스트는 없고, 3-1~3-3과 4번 테스트에서 함께 확인한다. 누수가 쌓이면 fork/exec를 반복하는 테스트에서 터진다.

## 문서
- [Anonymous Page › Page Cleanup](https://casys-kaist.github.io/pintos-kaist/project3/anon.html)

## 할 일
- [ ] `uninit_destroy`: 한 번도 로드되지 않은 페이지의 aux(`lazy_load_args`) free
- [ ] `anon_destroy`: `struct frame` free (frame table이 생기면 테이블에서도 제거)
- [ ] `lazy_load_segment` 성공 시 aux free (로드된 페이지는 anon으로 바뀌므로 `uninit_destroy`와 이중 free 없음)
- [ ] `supplemental_page_table_kill` → `process_exec` 재초기화 흐름 재확인

<!-- TESTS -->
