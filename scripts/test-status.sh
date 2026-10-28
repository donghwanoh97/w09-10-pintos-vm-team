#!/usr/bin/env bash
# 테스트 결과를 단계별로 묶어서 보여 주고, 기준 결과와 비교해 회귀(원래 PASS → 지금 FAIL)를 찾는다.
#
# 사용법 (pintos/vm/build 에서 make check 를 돌린 뒤):
#   ../../../scripts/test-status.sh                  단계별 요약
#   ../../../scripts/test-status.sh --save main.txt  현재 PASS 목록을 파일로 저장 (기준 만들기)
#   ../../../scripts/test-status.sh --diff main.txt  기준과 비교: 회귀 / 새로 통과
#
# 빌드 폴더를 직접 지정하려면 BUILD=경로 를 앞에 붙인다.

BUILD="${BUILD:-.}"
SRC="$BUILD/../.."

if ! ls "$BUILD"/tests >/dev/null 2>&1; then
  echo "tests 결과 폴더가 없음. pintos/vm/build 에서 실행하거나 BUILD=경로 를 지정할 것" >&2
  exit 1
fi

# 테스트 이름 → 단계
stage_of () {
  local t="$1"
  case "$t" in
    tests/vm/cow/*)                               echo "9 cow(추가)"; return ;;
    tests/threads/*)                              echo "0 threads"; return ;;
    tests/vm/pt-grow-*|tests/vm/pt-big-stk-obj)   echo "5 stack growth(10/30)"; return ;;
    tests/vm/mmap-*|tests/vm/lazy-file)           echo "6 mmap(10/30-31)"; return ;;
    tests/vm/swap-*|tests/vm/page-merge-*|tests/vm/page-parallel) echo "7 swap(11/1-2)"; return ;;
  esac
  # 소스(.c 또는 .inc)에서 fork 를 쓰면 fork 단계
  local base="$SRC/$t" files inc
  files="$base.c $base.inc"
  # .c 가 include 하는 .inc 도 같이 본다 (예: rox-multichild.c → rox-child.inc)
  for inc in $(grep -ho '#include "[^"]*\.inc"' "$base".c 2>/dev/null | cut -d'"' -f2); do
    files="$files $SRC/$inc"
  done
  if grep -qE 'fork *\(|exec_children' $files 2>/dev/null; then
    echo "4 fork(10/29)"; return
  fi
  echo "3 lazy·stack·보호(10/28)"
}

collect () {
  find "$BUILD"/tests -name '*.result' | sort | while read -r f; do
    t="${f#$BUILD/}"; t="${t%.result}"
    if grep -q '^PASS' "$f"; then echo "PASS $t"; else echo "FAIL $t"; fi
  done
}

case "$1" in
  --save)
    [ -z "$2" ] && { echo "저장할 파일 이름을 줄 것" >&2; exit 1; }
    collect | awk '$1=="PASS"{print $2}' > "$2"
    echo "PASS $(wc -l < "$2")개 저장: $2"
    ;;
  --diff)
    [ -f "$2" ] || { echo "기준 파일 없음: $2" >&2; exit 1; }
    now=$(collect)
    echo "== 회귀 (기준에서 PASS → 지금 FAIL)"
    reg=0
    while read -r t; do
      if echo "$now" | grep -qx "FAIL $t"; then echo "  $t"; reg=$((reg+1)); fi
    done < "$2"
    [ $reg -eq 0 ] && echo "  없음"
    echo "== 새로 통과 (기준에 없음 → 지금 PASS)"
    echo "$now" | awk '$1=="PASS"{print $2}' | grep -vxFf "$2" | sed 's/^/  /'
    [ $reg -eq 0 ] || exit 1
    ;;
  *)
    collect | while read -r r t; do echo "$(stage_of "$t")|$r|$t"; done | sort | awk -F'|' '
      { s=$1; sub(/^[0-9] /,"",s); if(!(s in tot)) keys[++n]=s; tot[s]++; if($2=="PASS") p[s]++; else f[s]=f[s] "\n    " $3 }
      END { for (i=1;i<=n;i++){ s=keys[i]; printf "%-24s %3d/%-3d%s\n", s, p[s]+0, tot[s], f[s] } }'
    ;;
esac
