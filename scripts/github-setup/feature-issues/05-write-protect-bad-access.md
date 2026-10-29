---
title: [3-3] 쓰기 보호·잘못된 접근
milestone: Anonymous Page: Lazy Loading
labels: type:feature, area:anon
tests: pt-write-code, pt-write-code2, pt-bad-read, bad-read, bad-write, bad-read2, bad-write2, bad-jump, bad-jump2
---
## 목표
잘못된 메모리 접근은 커널 PANIC이 아니라 프로세스 `exit(-1)`로 끝나게 한다.

## 문서
- [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html)

## 할 일
- [ ] 폴트 핸들러: present 페이지의 쓰기 폴트(`!not_present`)는 실패 처리
- [ ] 폴트 핸들러: `write && !page->writable`이면 실패 처리
- [ ] 유저 모드 폴트 처리 실패 시 `exit(-1)`
- [ ] **`read()` 시스템 콜 버퍼 검사에서 대상 페이지의 `writable` 확인** (pt-write-code2: `read()`로 코드 영역에 쓰면 커널 폴트 → PANIC)

## 참고
- pt-bad-read는 일정표상 5번(stack growth)에 있었지만 이 단계에서 통과하므로 여기로 옮겼다.

<!-- TESTS -->
