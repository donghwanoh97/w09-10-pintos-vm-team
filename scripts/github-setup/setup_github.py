#!/usr/bin/env python3
"""
일정표 기준으로 GitHub 이슈를 정리하는 스크립트.

하는 일
  1. 라벨(type:*, area:*)과 마일스톤(docs 섹션, 마감일 포함)을 만든다.
  2. feature-issues/*.md 를 읽어 기능 이슈를 만든다. (같은 제목이 있으면 재사용)
  3. 기존 테스트 이슈("[WEEK9-10] 테스트 - <이름>")를 기능 이슈의 sub-issue로 연결하고,
     마일스톤과 라벨(type:test, area:*)을 붙인다.
  4. --results 와 --close-passing 을 주면, make check 결과에서 pass인 테스트 이슈를
     댓글과 함께 닫는다. (기능이 없어도 우연히 통과하는 테스트는 제외)

기본은 dry-run(무엇을 할지 출력만)이고, --apply 를 줘야 실제로 반영한다.
같은 명령을 여러 번 돌려도 이미 있는 것은 건너뛴다. 기능 이슈 본문은 처음 만들 때만 쓰고,
이후에는 팀이 체크한 내용을 덮어쓰지 않는다. (--update-body 로 강제 가능)

필요한 것: gh CLI 로그인(gh auth login), 레포 쓰기 권한, Python 3.8+

사용 예 (레포 루트에서)
  python3 scripts/github-setup/setup_github.py              # dry-run
  python3 scripts/github-setup/setup_github.py --apply      # 반영
  python3 scripts/github-setup/setup_github.py --apply --results pintos/vm/build/results --close-passing
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys
import time

DEFAULT_REPO = "donghwanoh97/w09-10-pintos-vm-team"
ROOT = pathlib.Path(__file__).resolve().parent
FEATURE_DIR = ROOT / "feature-issues"
TESTS_MARKER = "<!-- TESTS -->"

# (제목, 마감일 KST, 설명)
MILESTONES = [
    ("Memory Management", "2026-10-27", "SPT, 프레임, 폴트 핸들러"),
    ("Anonymous Page: Lazy Loading", "2026-10-28", "lazy loading, 첫 스택 페이지, 쓰기 보호, page cleanup"),
    ("Anonymous Page: SPT Revisit", "2026-10-29", "fork (supplemental_page_table_copy)"),
    ("Stack Growth", "2026-10-30", "스택 성장"),
    ("Memory Mapped Files", "2026-10-31", "mmap / munmap"),
    ("Swapping", "2026-11-02", "eviction, anon/file swap"),
    ("마무리", "2026-11-03", "회귀 점검, 전체 make check"),
    ("Extra: Copy-on-Write", None, "여유 있을 때"),
]

LABELS = {
    "type:feature": ("1d76db", "일정표 단계(기능) 이슈"),
    "type:test": ("c5def5", "테스트 케이스 이슈"),
    "type:bug": ("d73a4a", "버그, PANIC, 회귀"),
    "area:spt": ("fbca04", "Supplemental Page Table"),
    "area:frame": ("fbca04", "Frame 관리"),
    "area:anon": ("0e8a16", "Anonymous Page"),
    "area:fork": ("0e8a16", "fork / SPT copy"),
    "area:stack": ("5319e7", "Stack Growth"),
    "area:mmap": ("b60205", "Memory Mapped Files"),
    "area:swap": ("e99695", "Swap In/Out"),
    "area:cow": ("bfdadc", "Copy-on-Write (Extra)"),
    "area:regression": ("cccccc", "이전 프로젝트 회귀 점검"),
}

# 기능이 없어도 기대 출력과 맞아서 통과하는 테스트. 결과로 자동 close 하지 않는다.
VACUOUS = {"mmap-ro", "mmap-zero", "pt-grow-bad", "swap-fork"}

TEST_TITLE_RE = re.compile(r"테스트\s*-\s*([A-Za-z0-9][A-Za-z0-9_-]*)")
WRITE_DELAY = 0.25  # GitHub secondary rate limit 회피


class GhError(RuntimeError):
    pass


def gh(method, path, body=None, allow_status=()):
    cmd = [
        "gh", "api", "-X", method, path,
        "-H", "Accept: application/vnd.github+json",
        "-H", "X-GitHub-Api-Version: 2022-11-28",
    ]
    data = None
    if body is not None:
        cmd += ["--input", "-"]
        data = json.dumps(body)
    proc = subprocess.run(cmd, input=data, capture_output=True, text=True)
    if proc.returncode != 0:
        m = re.search(r"HTTP (\d{3})", proc.stderr + proc.stdout)
        status = int(m.group(1)) if m else None
        if status in allow_status:
            return None
        raise GhError(f"{method} {path} 실패 (HTTP {status}): {proc.stderr.strip() or proc.stdout.strip()}")
    out = proc.stdout.strip()
    return json.loads(out) if out else None


def get_all(path):
    items, page = [], 1
    sep = "&" if "?" in path else "?"
    while True:
        chunk = gh("GET", f"{path}{sep}per_page=100&page={page}") or []
        items += chunk
        if len(chunk) < 100:
            return items
        page += 1


class Runner:
    def __init__(self, repo, apply):
        self.repo = repo
        self.apply = apply
        self.writes = 0

    def write(self, method, path, body, desc, allow_status=()):
        self.writes += 1
        if not self.apply:
            print(f"  [dry-run] {desc}")
            return None
        print(f"  {desc}")
        result = gh(method, f"repos/{self.repo}/{path}", body, allow_status)
        time.sleep(WRITE_DELAY)
        return result


def parse_feature(path):
    text = path.read_text(encoding="utf-8")
    m = re.match(r"---\n(.*?)\n---\n(.*)", text, re.S)
    if not m:
        raise ValueError(f"{path.name}: front matter(---) 형식이 아닙니다")
    meta = {}
    for line in m.group(1).splitlines():
        key, _, value = line.partition(":")
        meta[key.strip()] = value.strip()

    def split(v):
        return [x.strip() for x in (v or "").split(",") if x.strip()]

    return {
        "file": path.name,
        "title": meta["title"],
        "milestone": meta.get("milestone") or None,
        "labels": split(meta.get("labels")),
        "tests": split(meta.get("tests")),
        "body": m.group(2).rstrip() + "\n",
    }


def render_body(feature, test_issues):
    lines = []
    tests = feature["tests"]
    if tests:
        lines.append(f"## 통과시킬 테스트 ({len(tests)}개)")
        refs = []
        for name in tests:
            issue = test_issues.get(name)
            if issue:
                lines.append(f"- #{issue['number']} `{name}`")
                refs.append(f"Closes #{issue['number']}")
            else:
                lines.append(f"- `{name}` (테스트 이슈 없음)")
        if refs:
            lines += [
                "",
                "<details><summary>PR에 붙여넣기 (실제로 통과한 것만 남기기)</summary>",
                "",
                ", ".join(refs),
                "",
                "</details>",
            ]
    else:
        lines += ["## 통과시킬 테스트", "이 단계 전용 테스트 없음"]
    lines += ["", "<!-- generated by setup_github.py -->"]
    block = "\n".join(lines)
    body = feature["body"]
    return body.replace(TESTS_MARKER, block) if TESTS_MARKER in body else body + "\n" + block + "\n"


PATCH_NAMES = {"labels": "라벨", "milestone": "마일스톤", "body": "본문"}


def needed_patch(issue, want_labels, milestone_no):
    """이미 맞게 되어 있으면 빈 dict (불필요한 API 호출을 하지 않기 위해)."""
    have = {l["name"] for l in issue.get("labels", [])}
    patch = {}
    if not set(want_labels) <= have:
        patch["labels"] = sorted(have | set(want_labels))
    current_ms = (issue.get("milestone") or {}).get("number")
    if milestone_no and current_ms != milestone_no:
        patch["milestone"] = milestone_no
    return patch


def parse_results(path):
    results = {}
    for line in pathlib.Path(path).read_text().splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0].lower() in ("pass", "fail"):
            results[parts[1].rsplit("/", 1)[-1]] = parts[0].lower() == "pass"
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default=DEFAULT_REPO)
    ap.add_argument("--apply", action="store_true", help="실제로 반영 (없으면 dry-run)")
    ap.add_argument("--update-body", action="store_true", help="이미 있는 기능 이슈 본문도 다시 씀 (체크 상태 초기화됨)")
    ap.add_argument("--results", help="make check 후 생기는 build/results 경로")
    ap.add_argument("--close-passing", action="store_true", help="--results에서 pass인 테스트 이슈를 닫음")
    args = ap.parse_args()

    if args.close_passing and not args.results:
        ap.error("--close-passing 은 --results 와 함께 써야 합니다")

    run = Runner(args.repo, args.apply)
    repo = args.repo
    print(f"== 대상 레포: {repo}  ({'APPLY' if args.apply else 'DRY-RUN'})\n")

    features = [parse_feature(p) for p in sorted(FEATURE_DIR.glob("*.md"))]

    # 테스트 배정 중복 검사
    owner = {}
    for f in features:
        for t in f["tests"]:
            if t in owner:
                sys.exit(f"테스트 {t} 가 {owner[t]} 와 {f['file']} 에 중복 배정되어 있습니다")
            owner[t] = f["file"]

    # 1. 라벨
    print("== 라벨")
    existing_labels = {l["name"] for l in get_all(f"repos/{repo}/labels")}
    for name, (color, desc) in LABELS.items():
        if name not in existing_labels:
            run.write("POST", "labels", {"name": name, "color": color, "description": desc},
                      f"라벨 생성: {name}", allow_status=(422,))

    # 2. 마일스톤
    print("\n== 마일스톤")
    milestones = {m["title"]: m["number"] for m in get_all(f"repos/{repo}/milestones?state=all")}
    for title, due, desc in MILESTONES:
        if title in milestones:
            continue
        body = {"title": title, "description": desc}
        if due:
            body["due_on"] = f"{due}T14:59:59Z"  # KST 23:59:59
        created = run.write("POST", "milestones", body, f"마일스톤 생성: {title} (마감 {due or '-'})")
        milestones[title] = created["number"] if created else f"<new:{title}>"

    # 3. 이슈 목록
    issues = [i for i in get_all(f"repos/{repo}/issues?state=all") if "pull_request" not in i]
    by_title = {i["title"]: i for i in issues}
    test_issues = {}
    for i in sorted(issues, key=lambda x: x["number"]):
        m = TEST_TITLE_RE.search(i["title"])
        if not m:
            continue
        name = m.group(1)
        if name in test_issues:
            print(f"  ! 테스트 {name} 이슈가 여러 개입니다: #{test_issues[name]['number']}, #{i['number']} (앞의 것 사용)")
            continue
        test_issues[name] = i

    print(f"\n== 테스트 이슈 {len(test_issues)}개 확인, 기능 이슈 {len(features)}개 처리")
    missing = sorted(t for t in owner if t not in test_issues)
    unassigned = sorted(t for t in test_issues if t not in owner)
    if missing:
        print(f"  ! 레포에 이슈가 없는 테스트: {', '.join(missing)}")
    if unassigned:
        print(f"  ! 어느 기능에도 배정되지 않은 테스트 이슈: {', '.join(unassigned)}")

    # 4. 기능 이슈 + sub-issue
    for f in features:
        print(f"\n== {f['title']}  ({len(f['tests'])}개 테스트)")
        ms = milestones.get(f["milestone"]) if f["milestone"] else None
        if f["milestone"] and ms is None:
            print(f"  ! 마일스톤 '{f['milestone']}' 이 MILESTONES 목록에 없습니다")
        ms_value = ms if isinstance(ms, int) else None
        body = render_body(f, test_issues)
        parent = by_title.get(f["title"])

        if parent is None:
            payload = {"title": f["title"], "body": body, "labels": f["labels"]}
            if ms_value:
                payload["milestone"] = ms_value
            parent = run.write("POST", "issues", payload, f"기능 이슈 생성: {f['title']}")
        else:
            payload = needed_patch(parent, f["labels"], ms_value)
            if args.update_body:
                payload["body"] = body
            if payload:
                run.write("PATCH", f"issues/{parent['number']}", payload,
                          f"기능 이슈 #{parent['number']} {'/'.join(PATCH_NAMES[k] for k in payload)} 갱신")

        parent_no = parent["number"] if parent else None
        linked = set()
        if parent_no:
            linked = {s["id"] for s in get_all(f"repos/{repo}/issues/{parent_no}/sub_issues")}

        area = [l for l in f["labels"] if l.startswith("area:")]
        for name in f["tests"]:
            ti = test_issues.get(name)
            if not ti:
                continue
            patch = needed_patch(ti, ["type:test", *area], ms_value)
            if patch:
                run.write("PATCH", f"issues/{ti['number']}", patch,
                          f"#{ti['number']} {name}: {'/'.join(PATCH_NAMES[k] for k in patch)} 설정")
            if ti["id"] in linked:
                continue
            target = f"#{parent_no}" if parent_no else "(새 기능 이슈)"
            if parent_no or not args.apply:
                run.write("POST", f"issues/{parent_no}/sub_issues",
                          {"sub_issue_id": ti["id"], "replace_parent": True},
                          f"#{ti['number']} {name} → {target} sub-issue 연결")

    # 5. 통과한 테스트 닫기
    if args.results:
        results = parse_results(args.results)
        passed = {n for n, ok in results.items() if ok}
        print(f"\n== 결과 파일: {len(results)}개 중 {len(passed)}개 pass")
        to_close = sorted(
            (ti["number"], n) for n, ti in test_issues.items()
            if n in passed and n not in VACUOUS and ti["state"] == "open"
        )
        skipped = sorted(n for n in VACUOUS if n in passed and n in test_issues and test_issues[n]["state"] == "open")
        reopen = sorted(
            (ti["number"], n) for n, ti in test_issues.items()
            if n in results and not results[n] and ti["state"] == "closed"
        )
        if skipped:
            print(f"  (기능 없이 통과하는 테스트라 닫지 않음: {', '.join(skipped)})")
        if reopen:
            print("  ! 닫혀 있지만 지금 실패하는 테스트(회귀 의심): "
                  + ", ".join(f"#{no} {n}" for no, n in reopen))
        if args.close_passing:
            for no, name in to_close:
                run.write("POST", f"issues/{no}/comments",
                          {"body": f"`make check` 결과 `{name}` 통과 확인. (setup_github.py --close-passing)"},
                          f"#{no} {name}: 통과 댓글")
                run.write("PATCH", f"issues/{no}", {"state": "closed", "state_reason": "completed"},
                          f"#{no} {name}: 닫기")
        else:
            print(f"  닫을 수 있는 테스트 이슈 {len(to_close)}개 (--close-passing 을 주면 닫음)")

    print(f"\n== 완료: 쓰기 작업 {run.writes}건 {'반영함' if args.apply else '예정 (dry-run, --apply 로 반영)'}")


if __name__ == "__main__":
    try:
        main()
    except GhError as e:
        sys.exit(f"\n오류: {e}\n(gh auth status 로 로그인과 레포 쓰기 권한을 확인하세요)")
