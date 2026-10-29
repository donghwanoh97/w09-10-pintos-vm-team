---
title: [5] 스택 성장 (Stack Growth)
milestone: Stack Growth
labels: type:feature, area:stack
tests: pt-grow-stack, pt-grow-bad, pt-big-stk-obj, pt-grow-stk-sc, page-merge-stk
---
## 목표
스택 영역 근처의 폴트를 정상 스택 확장으로 판단해서 페이지를 새로 만들어 준다.

## 문서
- [Stack Growth](https://casys-kaist.github.io/pintos-kaist/project3/stack_growth.html)

## 할 일
- [ ] `vm_try_handle_fault`: SPT에 페이지가 없을 때 스택 확장 조건 검사
  - [ ] `addr >= rsp - 8`
  - [ ] `USER_STACK - (1 << 20) <= addr < USER_STACK` (최대 1MB)
- [ ] 유저 폴트는 `f->rsp`, 시스템 콜 중 커널 폴트는 시스템 콜 진입 시 저장해 둔 유저 rsp 사용
- [ ] `vm_stack_growth`: `pg_round_down(addr)`에 anon 페이지 할당 (+ claim)
- [ ] 시스템 콜 버퍼 검사(`check_address`)가 스택 확장 대상 주소를 막지 않는지 확인 (pt-grow-stk-sc)

## 참고
- pt-grow-bad는 스택 성장이 없어도 기대 결과(`exit(-1)`)와 맞아서 지금도 통과한다. 이 단계 PR이 머지될 때 닫는다.
- page-merge-stk는 일정표상 7-2에 있었지만 스택 성장이 핵심 조건이라 여기로 옮겼다. swap 이후에도 유지되는지는 7-2에서 확인한다.

<!-- TESTS -->
