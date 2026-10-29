# gdb-trace.sh 가 쓰는 기본 추적 지점. 코드를 고치지 않고 gdb dprintf 로 값을 찍는다.
# 형식: dprintf 함수이름,"출력 형식\n",인자...
# 인자 이름은 소스의 매개변수 이름과 같아야 한다. 필요한 지점을 자유롭게 추가/삭제할 것.

# 시스템 콜 입구: 어떤 시스템 콜이 불렸나
dprintf syscall_handler,"[gdb] syscall rax=%lld rdi=%#llx rsi=%#llx rdx=%lld\n",f->R.rax,f->R.rdi,f->R.rsi,f->R.rdx
# 시스템 콜 버퍼/주소 검사
dprintf check_address,"[gdb]   check_address %p\n",addr

# page fault 처리
dprintf vm_try_handle_fault,"[gdb] fault addr=%p user=%d write=%d not_present=%d\n",addr,user,write,not_present
# 예약 / 확보 / 지연 로딩
dprintf vm_alloc_page_with_initializer,"[gdb] alloc upage=%p type=%d writable=%d\n",upage,type,writable
dprintf vm_do_claim_page,"[gdb]   claim va=%p writable=%d\n",page->va,page->writable
dprintf lazy_load_segment,"[gdb]   lazy_load va=%p\n",page->va

# 정리
dprintf uninit_destroy,"[gdb] uninit_destroy va=%p\n",page->va
dprintf anon_destroy,"[gdb] anon_destroy va=%p\n",page->va

# 종료
dprintf sys_exit,"[gdb] sys_exit status=%d\n",status
