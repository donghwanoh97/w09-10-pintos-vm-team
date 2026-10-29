---
title: [8] 회귀 점검·마무리
milestone: 마무리
labels: type:feature, area:regression
tests: alarm-single, alarm-multiple, alarm-simultaneous, alarm-priority, alarm-zero, alarm-negative, priority-change, priority-donate-one, priority-donate-multiple, priority-donate-multiple2, priority-donate-nest, priority-donate-sema, priority-donate-lower, priority-fifo, priority-preempt, priority-sema, priority-condvar, priority-donate-chain, lg-create, lg-full, lg-random, lg-seq-block, lg-seq-random, sm-create, sm-full, sm-random, sm-seq-block, sm-seq-random, syn-remove
---
## 목표
VM 작업 때문에 이전 프로젝트 기능이 깨지지 않았는지 확인하고 최종 정리한다.

## 문서
- [FAQ](https://casys-kaist.github.io/pintos-kaist/project3/FAQ.html)

## 할 일
- [ ] 각 PR마다 `make check` 결과를 PR 본문 before/after 표에 기록
- [ ] 11/3: 전체 `make check`, 실패 테스트 수정
- [ ] FAQ 확인
- [ ] 11/4: 발표 자료

## 참고
- 아래 테스트는 지금 모두 통과하므로 바로 닫아도 된다. 다시 깨지면 이슈를 다시 열고 원인 PR을 링크한다.

<!-- TESTS -->
