---
title: [Extra] Copy-on-Write
milestone: Extra: Copy-on-Write
labels: type:feature, area:cow
tests: cow-simple
---
## 목표
fork 때 페이지를 바로 복사하지 않고, 쓰기 시점에 복사한다.

## 문서
- [Copy-on-Write (Extra)](https://casys-kaist.github.io/pintos-kaist/project3/cow.html)

## 할 일
- [ ] fork 시 부모·자식이 같은 frame을 읽기 전용으로 공유 (frame 참조 카운트)
- [ ] 쓰기 폴트(`!not_present && write`)에서 원래 writable이었던 페이지면 복사 후 매핑 교체 (`vm_handle_wp`)

<!-- TESTS -->
