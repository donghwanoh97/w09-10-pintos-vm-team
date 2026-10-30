상태: 공유 (써 보고 불편한 점은 댓글로)
관련 PR: #(도구 PR 번호)

# 한 줄 요약
- test-status.sh: make check 결과를 단계별로 묶고, main 대비 회귀(원래 PASS → 지금 FAIL)를 찾음
- dbg-test.sh: 테스트 하나를 다시 돌리고 기대 / 실제 / [dbg] 로그 / 차이 / 패닉 backtrace를 한 화면에 보여 줌
- gdb-trace.sh (실험): printf를 코드에 넣지 않고 gdb로 함수 호출과 인자를 자동으로 찍음

모두 pintos/vm/build 에서 실행함.

# 0. 스크립트 받기
스크립트는 저장소 맨 위 scripts/ 폴더에 있음.

    scripts/test-status.sh
    scripts/dbg-test.sh
    scripts/gdb-trace.sh
    scripts/trace/vm.gdb

도구 PR이 main에 머지된 뒤 (평소처럼 main 받기)

    git switch main
    git pull team main

머지 전에 미리 쓰고 싶으면 (내 브랜치는 그대로 두고 scripts 폴더만 가져옴)

    git fetch team eungi/debug-tools
    git checkout team/eungi/debug-tools -- scripts/

- 리모트 이름이 team이 아니면 git remote -v로 팀 저장소 이름을 확인해서 바꿀 것
- 실행 권한이 없다고 나오면: chmod +x scripts/*.sh
- 위 방법으로 가져온 scripts/는 내 브랜치에서 "새 파일"로 보임. 내 PR에 섞이지 않게 커밋하지 말 것 (main 머지 후에는 저절로 정리됨)
- 확인: 저장소 맨 위에서 ls scripts 했을 때 위 파일들이 보이면 됨

# 1. test-status.sh: 단계별 요약 + 회귀 확인

    make check
    ../../../scripts/test-status.sh

출력 예시

    threads                  18/18
    lazy·stack·보호(10/28)    65/66
        tests/vm/pt-write-code2
    fork(10/29)               0/17
    ...

회귀 확인 (PR 올리기 전)
1. main에서 make check 후 기준 저장: ../../../scripts/test-status.sh --save main.txt
2. 내 브랜치에서 make check 후 비교: ../../../scripts/test-status.sh --diff main.txt
3. "회귀 없음"이면 PR 테스트 칸에 그대로 붙임

- 단계는 일정표 기준. fork 단계는 테스트 소스(.c, .inc)에서 fork를 쓰는지로 판정함
- make 마지막의 "Error 1"은 실패 테스트가 있으면 make check가 일부러 내는 것. 빌드 에러 아님

# 2. dbg-test.sh: 테스트 하나 printf 디버깅

    ../../../scripts/dbg-test.sh tests/vm/pt-write-code2

- 커널에 찍는 로그는 "[dbg]"로 시작하게 씀
  예: printf ("[dbg] read buf=%p writable=%d\n", buffer, p ? p->writable : -1);
- [dbg] 줄은 테스트 출력과 순서대로 섞여서 나옴 → 어느 시점에 무슨 값이었는지 보임
- 커널 패닉이면 Call stack을 backtrace로 함수/줄 번호로 바꿔 보여 줌
- 이미 돌린 결과만 보기: --no-run
- 커밋 전에 남은 [dbg] 찾기: --check ("없음"이 나와야 함)

# 3. 디버깅 순서 (어디에 printf를 찍을지 정하는 법)
1. dbg-test.sh의 "차이"로 기대와 실제를 한 문장으로 적음
   예: 기대는 read에서 exit(-1), 실제는 read가 성공함
2. 실행 흐름에서 "막혔어야 할 지점"을 순서대로 나열함
   예: 시스템 콜 입구(check_buffer) → 커널이 실제로 씀 → (fault가 났다면) fault 핸들러
3. 지점마다 "왜 안 막혔지?"를 물어볼 값 하나씩 [dbg]로 찍음
4. 다시 돌려서 [dbg] 줄을 읽고, 원인 찾으면 --check로 지우고 커밋

예시: pt-write-code2

    [dbg] read buf=0x400000 page=0x8004243198 writable=0      ← 읽기 전용 페이지
    (pt-write-code2) survived reading data into code segment: FAILED   ← 그 사이 fault 없음

→ 읽기 전용인데 커널이 써도 fault가 안 남 (CR0_WP 꺼짐) → 시스템 콜 입구에서 writable 검사 필요
자세한 내용: [Troubleshooting] pt-write-code2

# 4. gdb-trace.sh (실험): printf 없이 자동 추적

    sudo apt-get install -y gdb      (처음 한 번)
    ../../../scripts/gdb-trace.sh tests/vm/pt-write-code2

- 추적 지점은 scripts/trace/vm.gdb에 있음
  (syscall_handler, check_address, vm_try_handle_fault, vm_alloc_page_with_initializer,
   vm_do_claim_page, lazy_load_segment, uninit_destroy, anon_destroy, sys_exit)
- 지점 추가: dprintf 함수이름,"[gdb] 설명 %p\n",인자이름 (인자 이름은 소스와 같아야 함)
- 테스트 출력과 추적 로그는 따로 나옴. 보통 실행보다 느림
- 아직 팀에서 써 본 적 없음. 안 되면 에러 화면을 댓글로

# 참고: 테스트 하나만 직접 돌리기

    rm -f tests/vm/pt-write-code2.result
    make tests/vm/pt-write-code2.result
    cat tests/vm/pt-write-code2.output
