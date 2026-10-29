---
title: [3-1] Lazy loading
milestone: Anonymous Page: Lazy Loading
labels: type:feature, area:anon
tests: lazy-anon, pt-bad-addr, page-linear, page-shuffle
---
## 목표
실행 파일 세그먼트를 첫 페이지 폴트 때 읽어오도록 바꾼다.

## 문서
- [Anonymous Page › Lazy Loading for Executable](https://casys-kaist.github.io/pintos-kaist/project3/anon.html)

## 할 일
- [ ] `vm_alloc_page_with_initializer`: uninit 페이지 생성, `writable` 설정, SPT 삽입
  - [ ] upage가 이미 있으면 **false** 반환
  - [ ] `spt_insert_page` 실패 시 page free
- [ ] `load_segment`: `lazy_load_args`를 aux로 전달
  - [ ] 오프셋은 `ofs += page_read_bytes` (남은 `read_bytes`를 더하면 두 번째 페이지부터 잘못 읽음)
- [ ] `lazy_load_segment`: `file_seek` → `file_read` → 나머지 0으로 채우기, 성공 시에도 aux free
- [ ] `anon_initializer`: `return true;`
- [ ] 시스템 콜 `check_address`를 `pml4_get_page` 대신 `spt_find_page` 기준으로 변경

## 참고
- page-linear, page-shuffle은 일정표상 swap(7-1) 테스트지만, 테스트 환경 메모리로는 이 단계에서 이미 통과한다. swap 구현 뒤에도 계속 통과하는지는 [7-1] 이슈에서 다시 확인한다.

<!-- TESTS -->
