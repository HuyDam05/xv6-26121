# Báo cáo cá nhân W04 – Vũ Khắc Hùng

**Dự án:** Crash Consistency and fsck trên xv6-riscv
**Nhóm:** 26121
**Thời gian:** 09/11–15/11/2026
**Nhiệm vụ:** N12 – Kiểm thử recovery khi log hỏng và hồi quy

## 1. Công việc thực hiện

- Chuyển các kiểm chứng log hỏng làm tay ở W03 thành ca test tự động trong `scripts/log_recovery_test.sh`.
- Script tự tạo ảnh crash ở điểm 2 (log đã commit, chưa install) và kiểm tra ảnh có `n > 0`, magic hợp lệ trước khi dùng, để không test trên ảnh không có log.
- Từ ảnh đó sao chép và sửa đúng một chỗ cho từng ca, boot lại bằng QEMU và kiểm tra kernel, shell và `fsck -n`.
- Kiểm tra `crash_ctl` với tham số sai.
- Chạy `crash_loop.sh` cho điểm crash 4 và 5.
- Chạy hồi quy `usertests -q` ở `CPUS=1` và `CPUS=4`.

## 2. Kết quả kiểm thử

### 2.1. Ca recovery (`bash scripts/log_recovery_test.sh`)

| Ca | Nội dung | Kỳ vọng | Kết quả |
|---|---|---|---|
| L00 | Đối chứng: ảnh crash nguyên vẹn | Replay (`recovering tail`), có `ct0`, `fsck` clean | PASS |
| L01 | log-flip: đảo 1 byte của log block đầu | `log: crc mismatch, dropping log`, không replay | PASS |
| L02 | log-magic: đảo 1 byte của magic | `log: bad magic, dropping log`, không replay | PASS |
| L03 | log-n: đặt `n = 255` (lớn hơn `LOGBLOCKS` = 30) | `log: bad header n=255, dropping log`, không replay | PASS |
| L04 | `crashtest 99 1` và `crashtest 6 1` | `invalid argument`, không crash, không panic | PASS |

Mỗi ca còn kiểm tra: không có `panic`, shell lên được (`init: starting sh`, `ls` thấy `README`), `ct0` có khi replay và không có khi bỏ log, và `fsck -n` báo clean. Kết quả chạy trên `main` sau khi PR #33 (`fsck` mới của Đức) được merge: 5 passed, 0 failed.

### 2.2. Crash loop điểm 4 và 5 (`CPUS=1`, 5 lần mỗi điểm)

| Điểm | Seed | Kết quả |
|---|---|---|
| 4 `DISK_WRITE` | 411–415 | 5/5 PASS (crashed, reboot_ok, fsck_exit = 0) |
| 5 `INSTALL_DONE` | 511–515 | 5/5 PASS (crashed, reboot_ok, fsck_exit = 0) |

Kết quả này chứng minh sau crash máy boot lại được và đĩa sạch. Báo cáo không khẳng định điểm 4 hay 5 luôn có replay, vì điểm 4 là lần ghi block thứ N nên có thể rơi vào giai đoạn ghi log, trước commit.

### 2.3. Hồi quy

| Lệnh | Kết quả |
|---|---|
| `make qemu CPUS=1`, `usertests -q` | ALL TESTS PASSED |
| `make qemu CPUS=4`, `usertests -q` | ALL TESTS PASSED |

## 3. Cơ chế kiểm tra

- Mỗi ca dùng bản sao của ảnh crash trong thư mục tạm, không sửa `fs.img` thật.
- Ảnh crash được kiểm tra `n > 0` và magic `0x4C4F4743` trước khi dùng. Nếu không crash được ở điểm 2, script dừng với mã lỗi 2 thay vì báo PASS giả.
- Sửa byte bằng cách đảo toàn bộ bit của byte, nên chắc chắn giá trị thay đổi.
- Script thoát khác 0 khi có ca FAIL, để gắn vào `make test` hoặc CI.
- Khi ca FAIL, script in lý do (ví dụ `thieu-thong-bao`, `replay-rac`, `fsck-khong-clean`) và lưu log vào `results/`.

## 4. Minh chứng

- `scripts/log_recovery_test.sh`: script kiểm thử (PR #34).
- `log-recovery-test.log`: kết quả chạy script (5 PASS).
- `crash_runs_w04_p4.csv`, `crash_runs_w04_p5.csv`: kết quả crash loop điểm 4 và 5.
- `anh/01-log-recovery-test.png`: kết quả script, 5 ca PASS.
- `anh/02-usertests-cpus4.png`: `usertests` pass ở `CPUS=4`.
- `anh/03-crash-loop-p4-p5.png`: bảng kết quả crash loop.
- `anh/04-pr34-merged-review.png`: PR #34 đã merge, CI xanh, có Approve của Đức.

PR: #34 (`test(log): add automated log recovery tests (N12)`), commit chính `b7349f1` (script) và `3f57479` (kết quả crash loop), merge ở `3b478b3`.

## 5. Công việc chưa hoàn thành

| Việc | Lý do | Kế hoạch |
|---|---|---|
| Nối `log_recovery_test.sh` vào `make test` | `make test` do Huy quản lý, chưa gọi script này | Nhờ Huy thêm lời gọi hoặc target riêng ở W05 |
| Dùng mode `log-*` của `fsck/corrupt.c` thay cho sửa byte bằng `dd` | Script hiện tự sửa byte để không phụ thuộc công cụ khác | Thay khi các mode `log-*` sẵn sàng |
| Ca `n` âm | L03 mới thử `n = 255` | Thêm ca `n = -1` |
| Chạy các ca log hỏng ở `CPUS=4` | Script chạy mặc định `CPUS=1` (có biến `CPUS` để đổi) | Chạy thêm ở `CPUS=4` ở W05 |
| Phân loại điểm 4 có replay hay không | Crash loop hiện chỉ ghi `crashed`, `reboot_ok`, `fsck_exit` | Ghi thêm số dòng `recovering tail` vào kết quả |

## 6. Kết luận

Đã tự động hóa ba nhánh bỏ log của `recover_from_log` (CRC lệch, magic sai, `n` ngoài khoảng) cùng ca đối chứng replay và ca tham số sai của `crash_ctl`. Cả 5 ca đạt trên `main` mới nhất, crash loop điểm 4 và 5 đạt 10/10, và `usertests` pass ở `CPUS=1` và `CPUS=4`. PR #34 đã được Đức review và merge.
