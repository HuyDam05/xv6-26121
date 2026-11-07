# Báo cáo Tuần 3 - Vũ Khắc Hùng

## 1. Mục tiêu

Tuần 3 tập trung hoàn thiện cơ chế chèn lỗi (crash-injection) trong đường commit của log, để có thể chủ động làm sập xv6 tại từng điểm và kiểm chứng việc khôi phục.

Các yêu cầu chính (H1, H2, H3 theo hướng dẫn tuần 3):

- H1: gắn hook crash vào các điểm còn thiếu trong `kernel/log.c`.
- H2: thêm syscall `crash_ctl(point, count)` để đặt điểm crash lúc chạy.
- H3: chứng minh CRC phát hiện log bị hỏng.
- Viết phần "Log và CRC" của design proposal.

---

## 2. Hiện trạng đầu tuần

Từ Tuần 2 đã có:

- `struct logheader` có `magic` và `crc`, `recover_from_log` kiểm tra magic và CRC (PR #11, #12, #15).
- Khung crash-injection (PR #13) nhưng mới có 1 điểm hoạt động là điểm 4 `CRASH_DISK_WRITE` trong `bwrite`. Các điểm 1 đến 3 mới được khai báo trong `crash.h`, chưa có hook.
- Điểm crash đặt lúc biên dịch, chưa có syscall.

---

## 3. Nội dung triển khai

### 3.1. H1 - Hook các điểm crash trong `kernel/log.c`

Thứ tự trong `commit()` sau khi sửa:

    write_log()
      [điểm 1] CRASH_COMMIT_BEFORE_HDR   log đã xuống đĩa, header chưa ghi
    write_head()                          commit point
      [điểm 2] CRASH_COMMIT_AFTER_HDR    đã commit, chưa install
    install_trans(0)
      [điểm 3] CRASH_INSTALL_MID         trong vòng lặp, sau mỗi block
      [điểm 5] CRASH_INSTALL_DONE        install xong, header vẫn n > 0
    n = 0, crc = 0, write_head()          xóa transaction

Điểm 4 `CRASH_DISK_WRITE` nằm trong `bwrite` (Tuần 2).

Lưu ý thiết kế:

- Điểm 3 chỉ gọi khi `recovering == 0`. Nếu không, kernel sẽ tự crash trong lúc khôi phục khi boot lại.
- `crash.h` thêm `CRASH_INSTALL_DONE` (điểm 5) và `CRASH_MAXPOINT` làm giới hạn hợp lệ cho `crash_ctl`.

### 3.2. H2 - Syscall `crash_ctl(point, count)`

- Số syscall: **23**. Số 22 đang bị `SYS_sync` và `SYS_psinfo` dùng chung (lỗi có sẵn trên `main`), nên `crash_ctl` không dùng số 22.
- `point` hợp lệ trong khoảng 0 đến `CRASH_MAXPOINT`. Giá trị ngoài khoảng trả lỗi, không crash.
- `count` là lần gọi thứ N của đúng điểm đó.
- `user/crashtest.c` mở rộng: `crashtest <point> <count>` đặt điểm crash rồi để workload chạy tiếp.

Các file đã sửa (8 file, +105 −3): `kernel/crash.h`, `kernel/log.c`, `kernel/syscall.c`, `kernel/syscall.h`, `kernel/sysproc.c`, `user/crashtest.c`, `user/user.h`, `user/usys.pl`.

---

## 4. Kiểm thử

### 4.1. Biên dịch

Lệnh: `make kernel/kernel fs.img fsck/fsck`

Kết quả: biên dịch thành công, exit code 0.

### 4.2. Tham số không hợp lệ

    $ crashtest 99 1
    crashtest: crash_ctl(99, 1) failed: invalid argument

QEMU không crash, vẫn còn dấu nhắc `$`.

### 4.3. Crash tại từng điểm

Mỗi lần bắt đầu từ ảnh đĩa sạch (`fs.img` sau `make`). Chạy `crashtest <điểm> 1` trong xv6, QEMU in `CRASH: point=N` rồi thoát. Sau đó boot lại trên cùng `fs.img` và chạy `./fsck/fsck -n fs.img`.

| Điểm | Tên | Khi boot lại | File tạo dở `ct0` | `fsck -n` |
|---|---|---|---|---|
| 1 | `COMMIT_BEFORE_HDR` | Không có `recovering tail`, log bị bỏ | Không có | clean |
| 2 | `COMMIT_AFTER_HDR` | `recovering tail 0..2` (dst 34, 47, 33) | Có | clean |
| 3 | `INSTALL_MID` | `recovering tail 0..2` (giống điểm 2) | Có | clean |
| 5 | `INSTALL_DONE` | `recovering tail 0..2` (giống điểm 2) | Có | clean |

Nhận xét:

- Crash trước commit point (điểm 1): transaction biến mất hoàn toàn, dữ liệu cũ nguyên vẹn.
- Crash sau commit point (điểm 2, 3, 5): transaction được replay đủ.
- Ở điểm 3 và 5, một số block đã được install trước khi crash nên bị ghi lại lần hai khi recovery, nhưng dữ liệu vẫn đúng. Như vậy replay có tính idempotent.

### 4.4. Hồi quy khi công tắc crash tắt

- `make qemu CPUS=1`, `usertests -q`: ALL TESTS PASSED.
- `make qemu CPUS=4`, `usertests -q`: ALL TESTS PASSED.

### 4.5. Kiểm tra CI

Lượt CI đầu của PR #26 báo lỗi ở bước `make fmt`: `clang-format` yêu cầu căn lề các dòng `#define SYS_...` trong `kernel/syscall.h` theo tên dài nhất (`SYS_crash_ctl`). Đã căn lại và đẩy bản sửa lên cùng nhánh. Sau đó CI `linux` và `macos` đều xanh.

### 4.6. H3 - CRC phát hiện log bị hỏng

Dùng ảnh đĩa lưu ngay sau khi crash ở điểm 2 (log đã commit, chưa install). Header log (block 2) có `n = 3`, `block[] = 34, 47, 33`, magic `0x4C4F4743` và một giá trị `crc`.

Làm hỏng ảnh: đảo toàn bộ bit của 1 byte trong log block đầu tiên (block 3, offset byte 10, từ `0x03` thành `0xfc`). Header được giữ nguyên nên `n` và magic vẫn hợp lệ, chỉ CRC phải phát hiện được sai lệch.

Boot lại trên ảnh hỏng (chạy QEMU trực tiếp để `make` không build lại `fs.img`):

    log: crc mismatch, dropping log
    init: starting sh
    $ ls        (không có file ct0)

Kết quả:

- Kernel in `crc mismatch, dropping log` và không có dòng `recovering tail` nào, nghĩa là không replay dữ liệu rác.
- File `ct0` do transaction bị hỏng tạo ra không xuất hiện.
- `./fsck/fsck -n fs.img` báo `fsck: clean`.

Kết luận: CRC bao gồm nội dung các log block. Chỉ một byte sai trong log block cũng đủ để transaction bị bỏ, ngay cả khi header hợp lệ.

### 4.7. Cách chạy lại kiểm thử

Trong xv6 (`make qemu`):

    crashtest 2 1

Sau khi QEMU thoát, boot lại không chạy `make clean` (để giữ `fs.img`):

    make qemu
    ./fsck/fsck -n fs.img

Với ảnh đĩa bị làm hỏng thủ công, chạy QEMU trực tiếp thay cho `make qemu`, vì `make` có thể build lại `fs.img` và ghi đè ảnh hỏng:

    qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic \
      -global virtio-mmio.force-legacy=false -drive file=fs.img,if=none,format=raw,id=x0 \
      -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

---

## 5. Phần Log và CRC trong proposal

Đã soạn mục "Thiết kế Log và CRC" cho `docs/proposal.md` (khoảng 0,75 trang): cấu trúc `logheader` (132 byte so với block 1024 byte), thứ tự commit và 5 điểm crash, bảng quyết định recovery theo `n`, magic, CRC, đặc tả `crash_ctl`, danh sách file sửa và bảng kết quả thực nghiệm ở mục 4.3.

---

## 6. Kết quả đạt được

- Hoàn thành H1: hook các điểm 1, 2, 3, 5 trong `kernel/log.c`, cộng điểm 4 từ Tuần 2 thành 5 điểm crash.
- Hoàn thành H2: syscall `crash_ctl` (số 23), kiểm tra tham số, `crashtest <point> <count>`.
- Mỗi điểm crash đã được kích hoạt trong QEMU, boot lại có log, `fsck -n` clean.
- `usertests -q` pass ở `CPUS=1` và `CPUS=4`.
- CI xanh sau khi căn lề `syscall.h`.
- Hoàn thành H3: đảo 1 byte trong log block của ảnh sau điểm 2, kernel in `crc mismatch, dropping log`, không replay, `fsck -n` clean.

## 7. Việc chưa xong, chuyển sang Tuần 4

| Việc | Lý do | Kế hoạch |
|---|---|---|
| Tự động hóa test log hỏng | H3 mới làm thủ công bằng `dd`. Mode `log-flip`, `log-magic`, `log-n` của `fsck/corrupt.c` (việc của Huy) chưa có | Đưa các ca log hỏng vào `make test` khi Huy có các mode `log-*` |
| Test trường hợp magic sai và `n` hỏng | Mới kiểm chứng nhánh CRC lệch | Làm nốt hai nhánh còn lại của bảng quyết định recovery |
| Sơ đồ Figma commit/recovery | Chưa vẽ | Vẽ sơ đồ thứ tự commit với 5 điểm crash |
| Review chéo cho PR #26 | PR đã được merge vào `main` bởi chính tác giả khi chưa có review của thành viên khác, chưa đúng quy trình của nhóm | Nhờ Huy hoặc Đức review bổ sung trên PR. Từ Tuần 4 chỉ merge sau khi có Approve |
| Số syscall 22 trùng giữa `sync` và `psinfo` | Thuộc việc dọn repo của Huy (C1), không nằm trong phạm vi PR này | Huy xóa `psinfo` sau khi PR #26 merge. PR #26 đã dùng số 23 cho `crash_ctl` |

---

## 8. Thông tin Git

- Repository: `HuyDam05/xv6-26121`
- Nhánh phát triển: `feat/hung-crash-hooks`
- Pull request: #26 (`feat(crash): hook diem crash 1,2,3,5 trong log.c va them syscall crash_ctl (H1, H2)`)
- Commit triển khai chính: `9262592` (kèm một commit căn lề `syscall.h` cho `clang-format`)
- Trạng thái PR #26: đã merge vào `main` (merge bởi `hungdzpro517`), CI xanh, chưa có review của thành viên khác
- Pull request proposal: #27 (`docs(proposal): add Log and CRC section - Hung`, nhánh `feat/hung-proposal-log`), CI xanh, đang chờ review
- Tài khoản GitHub: `hungdzpro517`
