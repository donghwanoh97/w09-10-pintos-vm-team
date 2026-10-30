상태: 해결
테스트: tests/vm/pt-grow-stk-sc
이슈: #(테스트 이슈 번호)   담당: @onekindalpha   해결 PR: #181
상세 기록: (Notion 링크)

## 증상
read 시스템 콜에서 바로 exit(-1). 나머지 스택 테스트(pt-grow-stack, pt-big-stk-obj, pt-grow-bad, pt-bad-read)는 PASS

    - (pt-grow-stk-sc) compare written data against read data
    - (pt-grow-stk-sc) end
    + pt-grow-stk-sc: exit(-1)

## 원인
read 버퍼가 아직 한 번도 안 건드린 스택 영역이라 SPT에 없었음.
check_address는 "SPT에 없으면 잘못된 주소"로 보고 exit(-1)을 함 → 스택을 키울 기회(page fault)까지 가지 못함.

- 테스트는 지역 배열 buf2[65536]을 만들고 read(handle, buf2 + 32768, slen)을 부름
- 함수에 들어갈 때 rsp가 한 번에 약 64KB 내려가고, 사용자 코드가 건드린 건 rsp 근처(배열 맨 아래) 한 페이지뿐
- 버퍼(buf2 + 32768)는 배열 가운데라 아직 SPT에 없음. 이 주소를 처음 건드리는 건 사용자가 아니라 read를 처리하는 커널

## 근거
1. 로그: check_address에서 "SPT에 없음"으로 죽음. 그 전에 fault 기록이 없음

       (pt-grow-stk-sc) read "sample.txt"
       [dbg] check_address 0x47477f88
       [dbg]   -> exit: not in spt (user_rsp=0x4746ff20)
       pt-grow-stk-sc: exit(-1)

2. 이 주소는 스택 성장 조건을 모두 만족함 → 막으면 안 되는 주소

   | 조건 | 값 | 결과 |
   |---|---|---|
   | USER_STACK - 1MB 이상 | 0x47477f88 ≥ 0x47380000 | O |
   | USER_STACK 미만 | 0x47477f88 < 0x47480000 | O |
   | user_rsp - 8 이상 | 0x47477f88 ≥ 0x4746ff18 | O (rsp보다 위, 이미 잡힌 프레임 안) |

3. 커널이 쓸 때 난 fault는 f->rsp를 쓸 수 없음
   - [코드] syscall-entry.S: 시스템 콜에 들어올 때 사용자 rsp를 저장하고 rsp를 커널 스택으로 바꿈
   - 그래서 시스템 콜 처리 중에 난 page fault의 f->rsp는 커널 스택 주소. 시스템 콜 입구에서 user_rsp를 따로 저장해 둬야 함
   - [문서] Stack Growth: "커널에서 페이지 폴트가 발생하면 f의 rsp는 사용자 스택 포인터가 아님. 사용자 모드에서 커널로 처음 전환될 때 rsp를 저장해 두는 방법이 필요"

## 해결
1. 시스템 콜 입구(syscall_handler 시작)에서 thread_current()->user_rsp = f->rsp 저장 (thread.h에 user_rsp 추가)
2. 스택 조건을 is_stack(f, addr, user) 하나로 묶음 (vm.c)
   - 1MB 안: USER_STACK - STACK_MAX <= addr < USER_STACK
   - rsp - 8 이상: PUSH는 rsp를 내리기 전에 8바이트 아래를 먼저 건드림
   - rsp는 사용자 모드 fault면 f->rsp, 커널 모드 fault면 user_rsp
3. vm_try_handle_fault: SPT에 없고 is_stack이면 vm_stack_growth(pg_round_down(addr)로 예약) → spt_find_page로 다시 찾아 claim
4. check_address: SPT에 없어도 is_stack(NULL, addr, false)면 통과 (syscall.c)

       if (spt_find_page (&thread_current ()->spt, (void *) addr) == NULL
           && !is_stack (NULL, (void *) addr, false))
           sys_exit (-1);

   - 통과하면 커널이 버퍼에 쓰다가 fault(user=0) → 3번 경로로 스택이 자람

       [dbg] check_address 0x47477f88                                  ← 통과
       [dbg] fault addr=0x47477f88 user=0 write=1 np=1 rsp=0x4746ff20  ← 커널 쓰기 fault
       [dbg] fault ok addr=0x47477f88
       (pt-grow-stk-sc) compare written data against read data

5. 확인용 [dbg] 출력을 지우고(dbg-test.sh --check) 다시 실행 → PASS. 스택 테스트 5개 PASS, main 대비 회귀 없음

## 디버깅 중 같이 만난 문제
- printf를 넣다가 if 중괄호가 빠짐 → return false가 항상 실행돼 첫 코드 fault(0x400de5)부터 실패, begin도 안 찍힘

      if (!is_user_vaddr (addr))
          printf ("[dbg] ...");
          return false;          ← if 밖

- 기능은 고쳐졌는데 FAIL: 차이 칸에 "+ exit(0)"만 있었음. dbg-test.sh는 [dbg] 줄을 빼고 차이를 보여 주지만, 채점(.result)은 [dbg] 줄까지 출력으로 봄 → printf 지우고 PASS

## 배운 것
- "SPT에 있나"만으로 주소를 거르면 아직 안 만든 스택이 막힘. 시스템 콜 입구 검사와 fault 핸들러는 같은 기준(is_stack)을 써야 함
- 커널 안에서 난 fault의 f->rsp는 사용자 rsp가 아님 → 입구에서 저장해 둔 값을 씀
- 차이 칸에 "-"가 없고 "+ exit(...)"만 있는데 FAIL이면 [dbg]가 남은 것. .result 파일로 확인
