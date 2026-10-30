상태: 제안 (10/28 리뷰에서 합의 예정)

![실패 처리와 자원 해제 규칙](이미지 주소)

근거 표시
- [문서]: KAIST 과제 문서
- [테스트]: .ck 파일
- [코드]: Pintos 코드에서 확인한 사실
- [제안]: 아직 합의 전인 제안 내용

# 1. 실패 처리 기준

실패가 생기면 "누구 때문에 생겼나"로 처리를 정한다. 위에서부터 순서대로 묻는다.

## 1-1. 사용자가 만들 수 있는 실패 → false/NULL 반환, exit(-1)은 입구에서만, ASSERT 금지
예: NULL 접근, 잘못된 주소, 읽기 전용 페이지에 쓰기

**근거**
- [문서] Memory Management: "페이지가 커널 가상 메모리에 있거나, 읽기 전용 페이지에 쓰기를 시도하는 경우, 해당 접근은 무효입니다. 무효 접근은 프로세스를 종료하고 모든 리소스를 해제합니다." → 끝내야 하는 건 프로세스 하나이지 OS 전체가 아니다.
- [테스트] bad-read.ck는 "bad-read: exit(-1)", pt-bad-addr.ck는 check_process_death로 프로세스 종료를 확인한다. 커널 패닉이 나면 실패한다.
- [코드] include/lib/debug.h에서 ASSERT는 조건이 거짓이면 PANIC을 부른다. 사용자가 만들 수 있는 값을 ASSERT로 검사하면 사용자 프로그램 하나 때문에 커널 전체가 멈춘다.
- [제안] 종료를 입구(page_fault, 시스템 콜)에서만 하는 이유
  - 아래 함수에서 바로 exit하면 받아 둔 자원(frame, page, lock)을 정리하지 못한다.
  - 같은 함수가 여러 곳에서 불리고 곳마다 처리가 다르다. (예: vm_do_claim_page는 fault 핸들러와 setup_stack에서 불림)
  - [코드] exception.c의 page_fault가 이미 이 구조다. vm_try_handle_fault가 false면 user 모드일 때 sys_exit(-1).

## 1-2. 우리 코드의 버그 → ASSERT
예: SPT에 없는 page를 remove, vm_get_frame이 page가 연결된 frame을 돌려줌

**근거**
- [제안] 우리 코드가 맞으면 절대 일어나지 않는 상태라, 일어났다면 바로 멈추고 위치를 알려 주는 게 디버깅에 가장 빠르다.
- [코드] 템플릿도 같은 방식이다. vm_get_frame 끝의 ASSERT(frame->page == NULL), vm_alloc_page_with_initializer의 ASSERT(VM_TYPE(type) != VM_UNINIT)

## 1-3. 진행할 방법이 없는 커널 자원 부족 → PANIC("이유")
예: hash_init 실패, struct frame malloc 실패

**근거**
- [문서] Frame Management: 스왑 공간이 가득 찬 경우 커널에 패닉을 발생시켜도 된다(복구 정책은 범위 밖). → 복구할 수 없는 자원 부족은 PANIC이 허용된다.
- [코드] 템플릿 주석: vm_get_frame은 "항상 유효한 주소를 돌려준다". 실패를 돌려줄 수 없는 함수다.
- [제안] hash_init 실패에 sys_exit(-1)을 쓰지 않는 이유
  - 사용자 프로그램 잘못이 아닌데 exit(-1)은 "프로그램이 잘못했다"는 뜻이 된다.
  - supplemental_page_table_init은 initd, __do_fork(자식), process_exec 세 곳에서 불리고 반환형이 void라 어디서 불렸는지 모른다. fork에서는 부모에게 -1을 알려야 하는데 자식 안에서 sys_exit하면 그 경로를 건너뛴다.
- [제안] ASSERT와 PANIC은 둘 다 커널을 멈추지만, 로그에서 "버그"인지 "자원 부족"인지 구분하려고 나눠 쓴다.

## 1-4. 다른 방법이 있는 실패 → false/NULL 반환
예: user pool 가득 참 (swap 단계에서 eviction으로 해결)

**근거**
- [문서] Frame Management: 빈 frame이 없으면 페이지를 내보내서(eviction) frame을 비운다. → 지금 실패해도 부른 쪽이 다른 방법을 시도할 수 있다.
- [제안] 지금은 vm_get_frame에서 PANIC("todo")로 막아 두고, swap 단계에서 vm_evict_frame으로 바꾼다.

# 2. 함수 안에서 부른 함수가 실패했을 때

## 2-1. 결과를 꼭 확인한다
예: spt_insert_page, pml4_set_page, swap_in, hash_init

**근거**
- [코드] 확인하지 않아서 생길 뻔한 문제
  - hash_init 결과를 안 보면, 실패해도 그대로 진행되다가 나중에 insert에서 엉뚱하게 죽는다.
  - pml4_set_page를 include 없이 부르면 반환형을 int로 가정해서 bool 결과를 잘못 읽을 수 있다. (빌드 경고 implicit declaration)

## 2-2. 내가 받은 것만, 받은 순서의 반대로 되돌린다
예: vm_do_claim_page
- 받은 순서: frame 받기 → 매핑 → 연결 → swap_in
- 매핑 실패: kva 반납 → frame 해제 → false
- swap_in 실패: 매핑 지우기 → 연결 끊기 → kva 반납 → frame 해제 → false
- kva 반납은 frame 해제보다 먼저 (frame을 free하면 frame->kva를 읽을 수 없음)

**근거**
- [제안] 매핑 실패 시에는 PTE도 연결도 없어서 나중에 아무도 이 frame을 해제해 주지 않는다. 직접 되돌리지 않으면 누수다.
- [제안] swap_in 실패 시 되돌리지 않으면 매핑이 살아 있어서 덜 채워진 페이지가 사용자에게 보인다.
- [코드] threads/mmu.c의 pml4_destroy(pt_destroy)는 매핑이 남아 있는 frame을 palloc_free_page로 반납한다. 그래서 매핑을 먼저 지우고(pml4_clear_page) kva를 반납해야 프로세스 종료 시 double free가 나지 않는다.
- [제안] 부른 함수 안에서 받은 자원은 그 함수가 정리한다. 함수 하나만 보고 자원이 맞는지 확인할 수 있다.

## 2-3. 정리한 다음 실패를 위로 돌려준다
- 여기서 exit하지 않는다. 끝낼지는 입구(page_fault, 시스템 콜)가 정한다.

**근거**
- [제안] 1-1의 근거와 같다. 자원과 lock을 놓고 나온 뒤에 종료해야 하고, 부르는 곳마다 처리가 다르다.
- [제안] swap 단계에서 fault 처리 중 frame table lock을 잡게 되면, 안에서 exit할 경우 process_exit이 같은 lock을 다시 잡으려다 멈출 수 있다.

## 2-4. 여러 함수를 거치는 자원은 해제 담당을 하나로 정한다
- 성공했을 때 해제하는 곳 하나, 실패했을 때 해제하는 곳 하나
- 예: lazy loading의 aux (load_segment가 malloc → lazy_load_segment가 사용 → 누가 free?)

**근거**
- [제안] 해제하는 곳이 둘이면 double free, 없으면 누수다.
- [문서] Anonymous Page의 supplemental_page_table_kill: "The caller handles the page table and physical memory." → 프로세스 종료 시 4KB 칸은 process_cleanup의 pml4_destroy가 반납한다. page destroy에서는 struct frame과 aux만 정리한다.
- [제안] aux는 상황이 세 가지라 각각 담당을 정해야 한다: lazy_load 성공, lazy_load 실패, 접근하지 않고 종료(uninit_destroy). → 10/28 리뷰에서 논의

# 계기
- [#152 리뷰](https://github.com/donghwanoh97/w09-10-pintos-vm-team/pull/152): "하지만 이를 ASSERT로 커널 범위에서 검사하게 되면 어떻게 될까요?" → 1-1
- [#152 리뷰](https://github.com/donghwanoh97/w09-10-pintos-vm-team/pull/152): "페이지 폴트가 발생한 경우에서 va는 페이지 정렬이 처리 된 주소 값일까요?" → spt_find_page 안에서 pg_round_down

# 관련 PR
- [#156 feat: SPT + Frame 구현](https://github.com/donghwanoh97/w09-10-pintos-vm-team/pull/156)
- (참고) #152, #154는 #156으로 통합되어 닫음

# 관련 이슈
- [#149 SPT 설계 및 구현](https://github.com/donghwanoh97/w09-10-pintos-vm-team/issues/149)
- [#150 프레임 및 폴트 핸들러 설계 및 구현](https://github.com/donghwanoh97/w09-10-pintos-vm-team/issues/150)
- [#65 bad-read](https://github.com/donghwanoh97/w09-10-pintos-vm-team/issues/65): NULL 접근 테스트. spt_find_page의 ASSERT(va)가 있으면 커널 패닉이 날 것을 리뷰에서 발견하고 제거 → 1-1의 계기
- [#74 pt-bad-addr](https://github.com/donghwanoh97/w09-10-pintos-vm-team/issues/74): 잘못된 주소 접근 시 false → page_fault에서 exit(-1)
