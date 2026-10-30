[Troubleshooting] 페이지 쓰는 법
- 테스트 이슈 댓글(3줄)로 끝나지 않은 것, 원인이 바로 안 보였던 것만 여기에 남김
- 한 항목 = 한 문제. 제목은 "테스트 이름: 증상 한 줄"
- 근거는 [코드] / [문서] / [로그]로 표시. 추측이면 "추측"이라고 씀
- 해결한 PR과 테스트 이슈를 꼭 링크

항목 템플릿

    ## 테스트 이름: 증상 한 줄
    - 증상: (dbg-test.sh "차이" 칸 그대로)
    - 원인:
    - 근거: [코드] / [문서] / [로그]
    - 해결:
    - 관련: PR #, 테스트 이슈 #

---

## pt-write-code2: read로 코드 영역에 써도 안 죽음
- 증상: read(handle, (void *) test_main, 1) 후에도 프로세스가 살아남음 (기대: exit(-1))
- 원인
  - check_buffer는 주소가 SPT에 있는지만 확인함 → 코드 페이지는 유효한 주소라 통과
  - 커널이 그 주소에 씀. 코드 페이지는 읽기 전용으로 매핑됐지만 CR0_WP가 꺼져 있어 커널(ring 0)의 쓰기는 하드웨어가 막지 않음 → page fault 없음
  - 그래서 vm_try_handle_fault의 writable 검사까지 갈 기회가 없음
- 근거
  - [로그] dbg-test.sh 결과
      [dbg] read buf=0x400000 page=0x8004243198 writable=0
      (pt-write-code2) survived reading data into code segment: FAILED
    read와 survived 사이에 fault 줄이 없음
  - [코드] threads/start.S에서 CR0에 PE, PG만 켬 (CR0_WP는 threads/loader.S에 정의만 있음)
- 해결: read처럼 커널이 버퍼에 쓰는 시스템 콜은 입구에서 page->writable 확인 → 아니면 exit(-1) (위키 실패 처리 규칙 1-1)
- 비교: pt-write-code(사용자가 직접 씀)는 ring 3이라 하드웨어가 막고, fault 핸들러 !not_present에서 false
- 관련: PR #159, 테스트 이슈 #

## write-normal, read-boundary: 정상 버퍼인데 시스템 콜에서 exit(-1)
- 증상: write/read 직후 바로 exit(-1)
- 원인: check_address가 pml4_get_page로 검사함. lazy 로딩이라 아직 접근 안 한 정상 페이지(전역 문자열 등)는 pml4에 매핑이 없어서 NULL → 잘못된 주소로 판단
- 근거
  - [코드] pml4_get_page는 PTE_P(지금 매핑돼 있나)만 봄. "유효한가"가 아님
  - [발제] VM에서 유효성 기준은 SPT ("spt에 있으면 밸리드")
- 해결: VM일 때 spt_find_page로 검사 (#ifdef VM, VM 아닐 때는 pml4_get_page 유지)
- 관련: PR #159

## 빌드 에러: field 'spt' has incomplete type (thread.h)
- 증상: filesys/page_cache.c 등에서 thread.h 128줄 struct supplemental_page_table spt; 에러
- 원인: vm.h 맨 위로 #include "threads/thread.h"를 옮김. thread.h와 vm.h가 서로 include해서, thread.h가 spt를 쓰는 시점에 구조체 정의가 아직 안 나옴
- 근거: [코드] vm.h는 #ifndef VM_VM_H로 두 번째 include를 건너뜀 → 정의 전에 thread.h가 읽힘
- 해결: #include "threads/thread.h"를 struct supplemental_page_table 정의 바로 아래(템플릿 원래 자리)로 되돌림

## make check 마지막의 "Error 1"
- 증상: N of 141 tests failed. 다음 줄에 make: *** ... Error 1
- 원인: 빌드 에러가 아님. tests/Make.tests의 check 규칙이 실패 테스트가 있으면 exit 1을 함
- 근거: [코드] tests/Make.tests 29줄 근처 "echo ... tests failed.; exit 1"
- 해결: 할 것 없음. 단계별로 보려면 scripts/test-status.sh

## make check TEST_SUBDIRS=... 했더니 undefined reference to run_test
- 증상: 커널 링크 에러
- 원인: TEST_SUBDIRS는 어떤 테스트를 돌릴지뿐 아니라 커널에 넣을 테스트 코드도 정함. tests/threads를 빼면 run_test가 빠짐
- 해결: 커널은 원래대로 빌드하고 .result만 골라서 make
    make -k $(cd ../.. && ls tests/userprog/*.ck tests/filesys/base/*.ck | grep -v '/null.ck' | sed 's/\.ck$/.result/')
- 참고: tests/userprog/null.ck는 소스 없는 남은 파일이라 제외해야 함
