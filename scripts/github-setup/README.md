# GitHub 이슈 정리 스크립트

팀 일정표의 **단계(기능)** 를 기능 이슈로 만들고, 기존 **테스트 이슈 141개** 를 그 하위(sub-issue)로 묶습니다.
docs를 보며 구현 → PR 머지 → 그 단계의 테스트 이슈가 닫히는 흐름입니다.

```
.github/ISSUE_TEMPLATE/
├── feature.md            # 기능(단계) 이슈 템플릿
└── bug.md                # 버그 이슈 템플릿
scripts/github-setup/
├── setup_github.py       # 라벨·마일스톤·기능 이슈 생성, sub-issue 연결, 통과 테스트 닫기
└── feature-issues/       # 기능 이슈 14개 본문 (front matter에 마일스톤·라벨·테스트 배정)
```

## 사용법

레포 루트에서 실행합니다. `gh auth login`(레포 쓰기 권한 계정)이 필요합니다.

```bash
# 1. 미리보기 (아무것도 바꾸지 않음)
python3 scripts/github-setup/setup_github.py

# 2. 반영: 라벨 12개, 마일스톤 8개, 기능 이슈 14개 생성 + 테스트 이슈 141개 연결
python3 scripts/github-setup/setup_github.py --apply

# 3. make check 결과에서 통과한 테스트 이슈 닫기
python3 scripts/github-setup/setup_github.py --apply \
    --results pintos/vm/build/results --close-passing
```

여러 번 돌려도 안전합니다. 이미 맞게 되어 있는 것은 건너뛰고, 기능 이슈 본문(체크리스트)은 처음 만들 때만 씁니다.

## 평소 운영

| 언제 | 할 일 |
|---|---|
| 단계 시작 | 해당 기능 이슈를 Assign |
| 구현 중 | 기능 이슈의 할 일 체크박스 갱신, Draft PR 미리 올리기 |
| 버그 발견 | `버그` 템플릿으로 이슈 생성 → 원인 찾으면 Wiki 트러블슈팅에 한 줄 |
| PR 올릴 때 | 기능 이슈 맨 아래 "PR에 붙여넣기"의 `Closes #…`에서 **실제로 통과한 것만** 남겨 PR 본문에 넣기 |
| PR 머지 후 | `Closes`에 적힌 테스트 이슈 자동으로 닫힘. 빠뜨린 게 있으면 3번 명령 한 번 더 |
| 회귀 발생 | 스크립트가 "닫혀 있지만 지금 실패하는 테스트"로 알려줌 → 이슈 다시 열고 원인 PR 링크 |

## 일정표와 달라진 테스트 배정

실제 `make check`에서 테스트가 통과하는 시점에 맞췄습니다. 합계는 141개 그대로입니다.

| 테스트 | 일정표 | 변경 | 이유 |
|---|---|---|---|
| exec-once, exec-arg, exec-missing, exec-bad-ptr, wait-bad-pid, rox-simple | 4 (fork) | 3-2 | fork 없이 통과 |
| syn-read, syn-write | 회귀 점검 | 4 (fork) | 내부에서 fork 사용 |
| page-linear, page-shuffle | 7-1 (swap) | 3-1 | 테스트 환경 메모리에선 swap 없이 통과. 7-1에서 회귀 확인 |
| page-parallel, page-merge-seq, page-merge-par | 7-1, 7-2 | 4 (fork) | spt_copy만 있으면 통과. 7-1에서 회귀 확인 |
| pt-bad-read | 5 (stack) | 3-3 | 이미 통과 |
| page-merge-stk | 7-2 | 5 (stack) | 스택 성장이 핵심 조건 |
| page-merge-mm | 7-2 | 6-1 (mmap) | mmap이 핵심 조건 |

**기능 없이도 통과하는 테스트** (`mmap-ro`, `mmap-zero`, `pt-grow-bad`, `swap-fork`)는 `--close-passing`이 닫지 않습니다. 해당 단계 PR의 `Closes`로 닫아주세요.

## 참고

- 테스트 배정을 바꾸려면 `feature-issues/*.md`의 `tests:` 줄을 고치고 다시 실행하면 됩니다. 이미 만든 기능 이슈 본문까지 다시 쓰려면 `--update-body` (체크 상태가 초기화되니 주의).
- 마일스톤 마감일은 KST 23:59로 들어갑니다. 이미 있는 마일스톤은 건드리지 않으니 날짜 변경은 GitHub에서 직접 하세요.
- sub-issue 연결은 GitHub REST API(`/issues/{n}/sub_issues`)를 씁니다. 테스트 이슈에 이미 다른 부모가 있으면 새 기능 이슈로 옮깁니다.
