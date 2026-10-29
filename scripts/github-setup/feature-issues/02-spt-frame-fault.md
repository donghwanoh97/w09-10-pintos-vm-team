---
title: [2] SPT + 프레임 + 폴트 핸들러
milestone: Memory Management
labels: type:feature, area:spt, area:frame
tests:
---
## 목표
SPT 기본 연산, 프레임 할당, 페이지 claim, 폴트 핸들러 골격을 만든다. 이 단계만으로 통과하는 테스트는 없고, 3단계에서 프로그램이 로드되면 함께 확인한다.

## 문서
- [Memory Management](https://casys-kaist.github.io/pintos-kaist/project3/vm_management.html)

## 할 일
- [ ] `supplemental_page_table_init`, `spt_find_page`, `spt_insert_page`, `spt_remove_page`
- [ ] `vm_get_frame` (`PAL_USER`, 실패 시 지금은 PANIC → swap 단계에서 eviction으로 교체)
- [ ] `vm_do_claim_page` (pml4 매핑 + `swap_in`), `vm_claim_page`
- [ ] `vm_try_handle_fault` (유저 주소, not_present, SPT 검색, writable)
- [ ] `supplemental_page_table_kill` + `process_exec`에서 SPT 재초기화

## 관련 PR
- #153, #155, #157

<!-- TESTS -->
