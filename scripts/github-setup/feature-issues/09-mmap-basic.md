---
title: [6-1] mmap 기본 동작
milestone: Memory Mapped Files
labels: type:feature, area:mmap
tests: mmap-read, mmap-write, mmap-close, mmap-unmap, mmap-twice, mmap-remove, mmap-clean, mmap-shuffle, mmap-ro, mmap-exit, lazy-file, page-merge-mm
---
## 목표
파일을 메모리에 매핑하고, 해제나 종료 시 변경 내용을 파일에 다시 쓴다.

## 문서
- [Memory Mapped Files](https://casys-kaist.github.io/pintos-kaist/project3/memory_mapped_files.html)

## 할 일
- [ ] `mmap` / `munmap` 시스템 콜 연결
- [ ] `do_mmap`: `file_reopen` 후 페이지마다 `VM_FILE` uninit 페이지 등록 (lazy)
- [ ] `do_munmap`: 매핑된 페이지 전체 해제
- [ ] `file_backed_initializer`, `file_backed_swap_in` (파일에서 읽기)
- [ ] `file_backed_destroy`: dirty 페이지 write-back, 매핑이 끝나면 file 닫기
- [ ] 프로세스 종료 시 남은 매핑 자동 해제 (mmap-exit)

## 참고
- mmap-ro는 mmap이 없어도 기대 결과와 맞아서 지금도 통과한다. 이 단계 PR이 머지될 때 닫는다.
- page-merge-mm은 일정표상 7-2에 있었지만 mmap이 핵심 조건이라 여기로 옮겼다.

<!-- TESTS -->
