---
title: [6-2] mmap 인자 검사·겹침·fork
milestone: Memory Mapped Files
labels: type:feature, area:mmap
tests: mmap-bad-fd, mmap-bad-fd2, mmap-bad-fd3, mmap-null, mmap-zero, mmap-zero-len, mmap-misalign, mmap-off, mmap-bad-off, mmap-kernel, mmap-overlap, mmap-over-code, mmap-over-data, mmap-over-stk, mmap-inherit
---
## 목표
잘못된 mmap 요청은 실패시키고, fork 시 파일 매핑도 자식에게 복사한다.

## 문서
- [Memory Mapped Files](https://casys-kaist.github.io/pintos-kaist/project3/memory_mapped_files.html)

## 할 일
- [ ] 실패 조건: `addr == 0`, page 정렬 안 된 addr, `length == 0`, offset 정렬 안 됨, 커널 주소, 콘솔 fd(0, 1), 잘못된 fd
- [ ] 기존 페이지(코드·데이터·스택·다른 매핑)와 겹치면 실패
- [ ] `supplemental_page_table_copy`에서 file page 복사 (mmap-inherit)

## 참고
- mmap-zero는 mmap이 없어도 기대 결과와 맞아서 지금도 통과한다. 이 단계 PR이 머지될 때 닫는다.

<!-- TESTS -->
