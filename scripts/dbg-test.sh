#!/usr/bin/env bash
# printf 디버깅용: 테스트 하나를 다시 돌리고 "기대 / 실제 / [dbg] 로그 / 차이 / 패닉 backtrace"를 한 화면에 보여 준다.
#
# 사용법 (pintos/vm/build 에서):
#   ../../../scripts/dbg-test.sh tests/vm/pt-write-code2        다시 실행 후 분석
#   ../../../scripts/dbg-test.sh --no-run tests/vm/pt-write-code2  이미 있는 .output만 분석
#   ../../../scripts/dbg-test.sh --check                         소스에 남은 [dbg] printf 위치 (커밋 전 확인용)
#
# 커널 코드에 찍는 로그는 "[dbg]"로 시작하게 쓴다. 예:
#   printf ("[dbg] read buf=%p writable=%d\n", buffer, p ? p->writable : -1);

SRC=../..
RUN=1

if [ "$1" = "--check" ]; then
  echo "== 소스에 남아 있는 [dbg] 출력 (커밋 전에 지울 것)"
  grep -rn '\[dbg\]' $SRC/vm $SRC/userprog $SRC/threads $SRC/include $SRC/filesys 2>/dev/null \
    | grep -v '/build/' || echo "  없음"
  exit 0
fi
[ "$1" = "--no-run" ] && { RUN=0; shift; }
[ -z "$1" ] && { sed -n '2,10p' "$0"; exit 1; }

for T in "$@"; do
  T="${T%.result}"; T="${T%.output}"; T="${T%.ck}"
  if [ ! -f "$SRC/$T.ck" ]; then echo "테스트 없음: $SRC/$T.ck" >&2; continue; fi

  echo "################ $T"
  if [ $RUN = 1 ]; then
    rm -f "$T.result" "$T.output" "$T.errors"
    if ! make "$T.result" > /tmp/dbg-make.log 2>&1; then
      if [ ! -f "$T.output" ]; then
        echo "== 빌드 또는 실행 실패 (make 로그 마지막 15줄)"; tail -15 /tmp/dbg-make.log; continue
      fi
    fi
  fi
  [ -f "$T.output" ] || { echo ".output 없음 (먼저 실행할 것)"; continue; }

  # 테스트 프로그램 실행 구간만 (부팅 메시지 제외)
  ACTUAL=$(sed -n "/^Executing '/,/^Execution of '/p" "$T.output" | sed '1d;$d')
  [ -z "$ACTUAL" ] && ACTUAL=$(cat "$T.output")

  echo "== 기대 (.ck)"
  EXPECTED=$(awk "/<<'EOF'/{f=1;next} /^EOF/{if(f){exit}} f" "$SRC/$T.ck")
  if [ -n "$EXPECTED" ]; then echo "$EXPECTED" | sed 's/^/  /'; else echo "  (.ck가 고정 출력 비교 형식이 아님: $SRC/$T.ck 직접 볼 것)"; fi

  echo "== 실제 (테스트 출력 + [dbg])"
  echo "$ACTUAL" | grep -E '^\(|\[dbg\]|: exit\(|PANIC|Page fault|Call stack|Kernel|Interrupt' | sed 's/^/  /'

  if [ -n "$EXPECTED" ]; then
    echo "== 차이 (- 기대 / + 실제, [dbg] 제외)"
    diff <(echo "$EXPECTED") <(echo "$ACTUAL" | grep -E '^\(|: exit\(') | grep '^[<>]' \
      | sed 's/^</  -/; s/^>/  +/' || true
  fi

  if grep -q "Call stack:" "$T.output"; then
    echo "== 패닉 backtrace"
    ADDRS=$(grep -m1 "Call stack:" "$T.output" | sed 's/.*Call stack://')
    if [ -x $SRC/utils/backtrace ]; then $SRC/utils/backtrace kernel.o $ADDRS | sed 's/^/  /'
    else echo "  backtrace kernel.o$ADDRS"; fi
  fi

  echo "== 결과"
  [ -f "$T.result" ] && sed 's/^/  /' "$T.result" | head -5
  echo
done
