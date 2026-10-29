---
title: [1] SPT 설계
milestone: Memory Management
labels: type:feature, area:spt
tests:
---
## 목표
테스트 분석 결과를 공유하고, SPT 자료구조와 `struct page` 필드를 팀 단위로 결정한다.

## 문서
- [Introduction](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html)
- [Memory Management](https://casys-kaist.github.io/pintos-kaist/project3/vm_management.html)

## 할 일
- [ ] 테스트 분석 공유 (어떤 기능이 어떤 테스트를 통과시키는지)
- [ ] SPT 자료구조 결정 (hash, 키는 `pg_round_down(va)`)
- [ ] `struct page` 추가 필드 결정 (`hash_elem`, `writable`, …)
- [ ] 결정 내용과 이유를 Wiki `설계-SPT` 페이지에 기록

## 관련 PR
- #153

<!-- TESTS -->
