---
title: [4] fork (supplemental_page_table_copy)
milestone: Anonymous Page: SPT Revisit
labels: type:feature, area:fork
tests: fork-once, fork-multiple, fork-recursive, fork-read, fork-close, fork-boundary, exec-boundary, exec-read, wait-simple, wait-twice, wait-killed, multi-recurse, multi-child-fd, rox-child, rox-multichild, syn-read, syn-write, page-parallel, page-merge-seq, page-merge-par
---
## 목표
fork한 자식이 부모의 주소 공간을 그대로 가지게 한다. 지금은 자식 SPT가 비어 있어서 첫 명령어에서 폴트 → `exit(-1)`이 된다.

## 문서
- [Anonymous Page › Supplemental Page Table - Revisit](https://casys-kaist.github.io/pintos-kaist/project3/anon.html)

## 할 일
- [ ] `supplemental_page_table_copy`
  - [ ] uninit 페이지: aux(`lazy_load_args`)를 malloc으로 복제 → 같은 type, `init`으로 `vm_alloc_page_with_initializer`
  - [ ] 로드된 페이지: `vm_alloc_page` + `vm_claim_page` → 부모 frame 내용 `memcpy`
  - [ ] 중간에 실패하면 false 반환
- [ ] 자식의 실행 파일을 `file_duplicate`로 복제하고 복제한 aux의 `file`도 교체 (지금은 부모의 `struct file *`를 공유해서, 부모가 먼저 종료하면 자식의 lazy load가 닫힌 파일을 읽음)

## 참고
- syn-read, syn-write는 일정표상 회귀 점검에 있었지만 내부에서 fork를 쓰므로 이 단계에서 통과한다.
- page-parallel, page-merge-seq, page-merge-par는 일정표상 swap 테스트지만 테스트 환경 메모리로는 이 단계에서 통과한다. swap 구현 뒤에도 유지되는지는 7-1에서 다시 확인한다.

<!-- TESTS -->
