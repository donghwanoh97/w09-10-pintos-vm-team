#!/usr/bin/env bash
# printf 를 코드에 넣지 않고 gdb dprintf 로 커널 함수 호출과 인자를 자동으로 찍는다.
#
# 사용법 (pintos/vm/build 에서):
#   ../../../scripts/gdb-trace.sh tests/vm/pt-write-code2
#   ../../../scripts/gdb-trace.sh tests/vm/pt-write-code2 내추적.gdb   (추적 지점 파일 직접 지정)
#
# 결과: 테스트 출력(.gdb.output)과 추적 로그(.gdb.trace)를 차례로 보여 준다.
# 필요: gdb (없으면 sudo apt-get install -y gdb)

DIR="$(cd "$(dirname "$0")" && pwd)"
T="${1%.result}"; T="${T%.output}"
TRACE="${2:-$DIR/trace/vm.gdb}"
PORT=1234

[ -z "$T" ] && { sed -n '2,10p' "$0"; exit 1; }
command -v gdb >/dev/null || { echo "gdb 없음: sudo apt-get install -y gdb" >&2; exit 1; }
[ -f "$TRACE" ] || { echo "추적 파일 없음: $TRACE" >&2; exit 1; }

# 1. 커널과 테스트 프로그램 빌드 + 실제 pintos 명령 얻기
rm -f "$T.result" "$T.output" "$T.errors"
make -s >/dev/null 2>&1 || { echo "커널 빌드 실패: make 로 에러 확인" >&2; exit 1; }
make -s "$T" >/dev/null 2>&1 || { echo "테스트 프로그램 빌드 실패: make $T" >&2; exit 1; }
CMD=$(make -n "$T.result" | grep '^pintos' | head -1)
[ -z "$CMD" ] && { echo "pintos 실행 명령을 찾지 못함" >&2; exit 1; }
# 출력 리다이렉트 제거, --gdb 추가, 타임아웃 늘리기 (gdb가 멈췄다 가므로 느림)
CMD=$(echo "$CMD" | sed 's/ *< *\/dev\/null.*$//; s/^pintos /pintos --gdb /; s/-T [0-9]*/-T 600/')

echo "== 실행: $CMD"
eval "$CMD" > "$T.gdb.output" 2>&1 &
QPID=$!
sleep 2

# 2. gdb 연결 후 dprintf 지점을 걸고 끝날 때까지 진행
gdb -q -batch -nx \
  -ex "set pagination off" -ex "set confirm off" \
  -ex "target remote localhost:$PORT" \
  -x "$TRACE" \
  -ex "continue" \
  kernel.o 2>&1 | grep -E '^\[gdb\]|No symbol|not defined|Cannot' > "$T.gdb.trace"

wait $QPID 2>/dev/null

echo "== 테스트 출력"
sed -n "/^Executing '/,/^Execution of '/p" "$T.gdb.output" | sed '1d;$d' \
  | grep -E '^\(|: exit\(|PANIC|Call stack' | sed 's/^/  /'
echo "== 추적 로그 ($TRACE)"
if [ -s "$T.gdb.trace" ]; then sed 's/^/  /' "$T.gdb.trace"; else echo "  (없음: gdb 연결 실패 또는 지점 이름 오류)"; fi
echo
echo "전체 파일: $T.gdb.output, $T.gdb.trace"
