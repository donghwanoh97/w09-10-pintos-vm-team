상태: 해결
테스트: tests/vm/pt-write-code2
이슈: #77   담당: @onekindalpha   해결 PR: #159
상세 기록: https://app.notion.com/p/szloveleesz/4074d1a35702807c81e5faa626abeeeb

## 증상
read(handle, (void *) test_main, 1) 후에도 프로세스가 살아남음

    - pt-write-code2: exit(-1)
    + (pt-write-code2) survived reading data into code segment: FAILED
    + pt-write-code2: exit(1)

## 원인
테스트는 "파일 내용을 코드 영역에 읽어 넣어 줘"라고 read를 부름.
코드 영역은 읽기 전용이지만, 실제로 쓰는 건 커널이라 막히지 않았음.
- 사용자 프로그램이 읽기 전용 페이지에 쓰면 CPU가 막음 (page fault)
- 커널이 쓰면 CPU가 막지 않음 (Pintos는 "커널도 읽기 전용을 지키게 하는 스위치"를 켜지 않음)
- 시스템 콜 입구(check_buffer)는 "주소가 있나"만 보고 "쓸 수 있나"는 안 봤음

## 근거
1. 로그: 버퍼가 읽기 전용 페이지였고, 커널이 써도 fault가 안 남

       [dbg] read buf=0x400000 page=0x8004243198 writable=0      ← read 버퍼가 읽기 전용 페이지
       (pt-write-code2) survived reading data into code segment: FAILED

   read 줄과 survived 줄 사이에 page fault 기록이 없음 → 커널의 쓰기를 아무도 막지 않았음

2. 코드 영역이 왜 읽기 전용인가: 실행 파일을 보면 코드 부분의 권한이 "읽기 + 실행"(R E)이고 쓰기(W)가 없음.
   그래서 load_segment가 이 페이지를 writable=false로 등록함

       readelf -lW tests/vm/pt-write-code2
       LOAD ... 0x400000 ... R E     ← 코드 (쓰기 없음)
       LOAD ... 0x405ea0 ... RW      ← 데이터 (쓰기 가능)

3. 커널은 왜 안 막히나: CPU에는 "커널도 읽기 전용을 지킬지" 정하는 스위치(CR0_WP)가 있음.
   Pintos는 부팅할 때(threads/start.S) 이 스위치를 켜지 않음 → 커널은 읽기 전용 페이지에도 그냥 씀

## 해결
1. 디버그 로그로 버퍼 페이지의 writable 값을 찍어서, 읽기 전용(writable=0)인데도 커널 쓰기가 통과하는 걸 확인함
2. userprog/syscall.c의 SYS_READ에서 check_buffer 바로 아래에, 버퍼 주소로 SPT에서 page를 찾아 writable이 false면 sys_exit(-1) 하는 검사를 넣음

       check_buffer(buffer, size);
       #ifdef VM
       struct page *bp = spt_find_page (&thread_current ()->spt, buffer);
       if (bp != NULL && !bp->writable)
           sys_exit(-1);
       #endif

   - check_buffer 뒤에 둔 이유: 주소가 NULL이거나 커널 영역이면 check_buffer가 먼저 걸러 줌
   - fd 0(키보드) 분기보다 앞에 둬서 키보드 입력 경로도 같이 막힘
   - #ifdef VM: SPT는 VM 빌드에만 있어서 userprog 빌드가 깨지지 않게 함
3. 확인용 디버그 출력을 지우고(dbg-test.sh --check) 다시 실행 → pt-write-code2 PASS
4. 남은 점: 지금은 버퍼의 첫 페이지만 확인함. 여러 페이지에 걸친 버퍼는 페이지마다 확인하도록 확장 예정 (PR #159 남은 작업)

## 배운 것
- 사용자가 직접 쓰는 건 CPU가 막아 주지만, 커널이 대신 쓰는 건 막아 주지 않음 → 그런 경로는 우리가 직접 검사해야 함
- "주소가 유효한가"와 "쓸 수 있는가"는 다른 검사
