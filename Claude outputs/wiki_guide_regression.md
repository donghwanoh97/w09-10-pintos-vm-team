PR 올리기 전에 "원래 통과하던 테스트가 내 변경 때문에 깨졌는지"를 확인하는 방법.
도구: scripts/test-status.sh

# 왜 하나
- make check는 "몇 개 실패"만 알려 줌
- 그 안에는 원래부터 실패하던 것(아직 안 만든 fork, mmap 등)과 내가 새로 깨뜨린 것이 섞여 있어서 구분이 안 됨
- 그래서 main(내 변경 전)에서 통과하던 목록을 먼저 저장해 두고, 내 브랜치 결과와 비교함
- 비교하면 "main에서는 통과했는데 지금 실패하는 것" = 내가 깨뜨린 것만 나옴

# 언제 하나
- PR 올리기 직전
- 공용 함수(vm.c, syscall.c, process.c 등)를 고친 직후

# 0. 준비
모든 명령은 pintos/vm/build 에서 실행함.

    cd /workspaces/w08-pintos-vm-team/pintos/vm/build

scripts/ 폴더가 없으면 main을 받음 (git pull team main)

# 1. main에서 먼저 돌려 두기
내 변경 전에는 무엇이 통과했는지 알아야 비교할 수 있음. 그 목록을 main.txt로 저장함

    git switch main
    git pull team main
    cd pintos/vm/build
    make clean
    cd .. && make && cd build
    make check
    ../../../scripts/test-status.sh --save main.txt

- main이 바뀌면(다른 PR 머지) 다시 만듦

# 2. 내 브랜치 돌리기

    git switch 내-브랜치
    cd pintos/vm/build
    make clean
    cd .. && make && cd build
    make check

- make clean을 해도 main.txt는 지워지지 않음

# 3. 비교

    ../../../scripts/test-status.sh --diff main.txt

출력 예시

    == 회귀 (기준에서 PASS → 지금 FAIL)
      없음
    == 새로 통과 (기준에 없음 → 지금 PASS)
      tests/vm/pt-write-code2

- 회귀: 원래 통과했는데 지금 실패 → 내 변경 때문. 그 테스트를 다시 봄
- 새로 통과: 이번 작업으로 고친 테스트

# PR에 적을 것

    - main 대비 회귀: 없음 (test-status.sh --diff)

# 자주 나는 문제

**"test-status.sh: No such file or directory" 또는 "No rule to make target 'clean'"**
- 원인: build 폴더가 아닌 곳에서 실행함
- 해결: cd pintos/vm/build 후 다시

**"N of 141 tests failed." 다음 줄에 "Error 1"**
- 원인: 빌드 에러 아님. 실패 테스트가 있으면 make check가 일부러 냄
- 해결: 무시하고 3번(비교)으로

**회귀에 alarm-simultaneous 같은 타이머 테스트가 가끔 나옴**
- 원인: 타이밍에 따라 가끔 실패함
- 해결: 그 테스트만 다시 돌림

      rm -f tests/threads/alarm-simultaneous.result
      make tests/threads/alarm-simultaneous.result

**결과가 예전 그대로임**
- 원인: 결과 파일이 남아 있어서 다시 안 돌림
- 해결: make clean 후 다시
