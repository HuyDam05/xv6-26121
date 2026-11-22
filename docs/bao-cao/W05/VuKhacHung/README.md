# W05 N17 - Vũ Khắc Hùng

## 1. Mục tiêu

Hoàn thiện kiểm thử recovery của log khi header hoặc nội dung log bị hỏng:
bổ sung ca `n` âm, chạy bộ kiểm thử ở CPUS=1 và CPUS=4, và chạy crash loop
cho điểm crash 4 và 5 với số dòng `recovering_lines`.

## 2. Triển khai

- Thêm ca L05 vào `scripts/log_recovery_test.sh`: ghi `n = -1`
  (`ff ff ff ff`) tại đầu log header của ảnh crash điểm 2.
- Chạy bộ L00-L05 ở CPUS=1 và CPUS=4, lưu log tổng và log boot của từng ca.
- Chạy `scripts/crash_loop.sh`:
  - điểm 4: CPUS=1, `NMAX=6`, `SEED_BASE=621`, 10 lượt;
  - điểm 5: CPUS=4, `NMAX=6`, `SEED_BASE=721`, 10 lượt.
- Chạy hồi quy `usertests` bằng `./test-xv6.py usertests`.
- Không sửa mã kernel. Script vẫn tiêm byte bằng `dd`/`xxd`,
  chưa dùng mode `log-*` của `corrupt`.

## 3. Kết quả kiểm thử

### 3.1. Bộ log recovery L00-L05

| Ca | Nội dung | Kỳ vọng | CPUS=1 | CPUS=4 |
|---|---|---|---|---|
| L00 | Ảnh crash nguyên vẹn | Replay (`recovering tail`) | PASS | PASS |
| L01 | Sai 1 byte log block | `crc mismatch`, không replay | PASS | PASS |
| L02 | Sai magic | `bad magic`, không replay | PASS | PASS |
| L03 | `n = 255` | `bad header n=255`, không replay | PASS | PASS |
| L04 | `crash_ctl` tham số sai | `invalid argument`, không crash | PASS | PASS |
| L05 | `n = -1` | `bad header n=-1`, không replay | PASS | PASS |

Mỗi ca kiểm tra thêm: không panic, shell lên được, `fsck -n` clean.
Kết quả: 6 passed, 0 failed ở cả hai cấu hình.

### 3.2. Crash loop điểm 4 và 5

| Điểm | Cấu hình | Số lượt | PASS | Lượt có recovery | Panic |
|---|---|---|---|---|---|
| 4 (`bwrite`) | CPUS=1 | 10 | 10 | 4 | 0 |
| 5 (sau install, trước xóa header) | CPUS=4 | 10 | 10 | 10 | 0 |

Quan sát:

- Điểm 4: trong 10 lượt này, recovery chỉ xuất hiện khi N = 5 hoặc 6
  (3 dòng `recovering`); N từ 1 đến 4 không có recovery. Không phải
  crash nào ở điểm 4 cũng tạo recovery.
- Điểm 5: cả 10 lượt đều có recovery (3 dòng), vì khi crash header
  vẫn còn `n > 0` và CRC hợp lệ.
- Nguyên nhân điểm 4 chỉ có recovery từ N = 5 chưa được kiểm chứng
  bằng mã nguồn.

## 4. Regression

- `./test-xv6.py usertests`: ALL TESTS PASSED (cấu hình CPUS mặc định;
  log không ghi số CPU).
- `log-cpus1.log` và `log-cpus4.log` có nội dung giống nhau vì script
  không in số CPU. Cấu hình CPUS=4 phân biệt qua các dòng
  `hart N starting` trong `cpus4/L0x.boot.log`.

## 5. Minh chứng

- `log-cpus1.log`, `log-cpus4.log`
- `cpus1/L0x.boot.log`, `cpus4/L0x.boot.log`
- `crash-point4-cpus1.csv`, `crash-point5-cpus4.csv`
- `usertests.log`
- PR #39: `test(log): add L05 negative n and run log suite on 1 and 4 CPUs (W05 N17)`,
  đã được Đức review và merge.

## 6. Giới hạn và công việc tiếp theo

- Mới thử `n = -1` cho giá trị âm; chưa thử các giá trị âm khác.
- Mode `log-flip`, `log-magic`, `log-n` trong `fsck/corrupt.c` chưa có;
  việc này thuộc phối hợp với Huy.
- Việc nối `log_recovery_test.sh` vào `make test` thuộc N14 của Huy.
- Crash điểm 4 chỉ chạy ở CPUS=1 và điểm 5 chỉ chạy ở CPUS=4,
  mỗi điểm 10 lượt; chưa so sánh chéo hai cấu hình.
- Chưa chứng minh tính đúng đắn với mọi thời điểm crash, chưa chạy
  200+ lần crash có seed.
- Cần ghi số CPU vào log của `usertests` để bằng chứng tự giải thích.
