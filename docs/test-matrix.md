# Test matrix - Đề tài 26121 (Crash consistency và fsck)

Cập nhật: 25/10/2026. Bản sơ bộ, mọi dòng đang ở trạng thái Planned. Cột "Cách chạy" sẽ cập nhật khi có script.
Quy ước điểm crash: (1) Sau write_log, trước write_head.
(2) Sau write_head (commit point).
(3) Giữa install_trans.
(4) Khi ghi block trong bwrite.
(5) Sau install_trans, trước khi xóa log header.

Cập nhật: 15/11/2026.

W04: Đã triển khai 15 ca kiểm thử tự động trong make test.
Crash-loop đã kiểm chứng CPUS=1/4, bao gồm N mở rộng
từ 1 đến 6. Các trường hợp chưa chạy giữ trạng thái Planned.

| ID | Nhóm | Kịch bản | Kết quả mong đợi | Cách chạy | Trạng thái |
|---|---|---|---|---|---|
| L01 | Log | Crash tại điểm (1) | Transaction không có hiệu lực, boot lại không redo, fsck clean | crash_loop | Planned |
| L02 | Log | Crash tại điểm (2) | Recovery redo đủ, dữ liệu transaction có mặt, fsck clean | crash_loop | Planned |
| L03 | Log | Crash tại điểm (3) | Recovery redo lại từ đầu, fsck clean | crash_loop | Planned |
| L04 | Log | Crash tại điểm (4) | Redo lặp lại không làm hỏng dữ liệu, fsck clean | crash_loop | Planned |
| L05 | Log | Crash tại điểm (5) | Recovery đưa bitmap/inode về trạng thái nhất quán | crash_loop | Planned |
| L06 | Log/CRC | Commit bình thường, boot lại | Không có cảnh báo CRC | usertests | Planned |
| L07 | Log/CRC | Sửa 1 byte trong log block, giữ n>0 | CRC lệch, bỏ transaction, in cảnh báo, không redo dữ liệu rác | corrupt+boot | Planned |
| L08 | Log/CRC | Header sai magic | Recovery từ chối header, in cảnh báo | corrupt+boot | Planned |
| L09 | Log/CRC | Header có n lớn hơn LOGBLOCKS | Recovery từ chối, không tràn mảng, không panic | corrupt+boot | Planned |
| L10 | Log/CRC | Header ghi dở (cắt nửa) | CRC lệch, bỏ transaction | corrupt+boot | Planned |
| L11 | Log/CRC | crc32("123456789") và chuỗi rỗng | 0xCBF43926 và 0x00000000 | unit (host) | Planned |
| C01 | Crash-injection | Công tắc tắt (CRASH_NONE) | Kernel chạy bình thường, usertests pass | usertests | Planned |
| C02 | Crash-injection | crash_ctl với điểm hoặc số lần không hợp lệ | Trả lỗi, không crash | usertests | Planned |
| C03 | Crash-injection | Crash sau đúng N lần ghi block | Dừng đúng N, cùng N cho cùng kết quả | crash_loop | Planned |
| F01 | fsck | Ảnh gốc | fsck -n báo clean, exit 0 | fsck | Planned |
| F02 | fsck | Superblock sai magic hoặc size | Báo lỗi, exit khác 0 | corrupt+fsck | Planned |
| F03 | fsck | Bitmap: bit 1 thành 0 (block đang dùng bị coi là free) | Phát hiện, -r sửa, chạy lại clean | corrupt+fsck | Planned |
| F04 | fsck | Bitmap: bit 0 thành 1 (leak) | Phát hiện, -r sửa | corrupt+fsck | Planned |
| F05 | fsck | nlink lệch so với số dirent | Phát hiện, -r ghi lại nlink | corrupt+fsck | Planned |
| F06 | fsck | Hai inode cùng trỏ một block | Phát hiện, báo lỗi | corrupt+fsck | Planned |
| F07 | fsck | Inode mồ côi (type khác 0, không dirent) | Phát hiện, -r đưa vào /lost+found | corrupt+fsck | Planned |
| F08 | fsck | Thư mục thiếu . hoặc .. | Phát hiện | corrupt+fsck | Planned |
| F09 | fsck | Dirent trỏ inode rỗng hoặc ngoài phạm vi | Phát hiện, -r xóa dirent | corrupt+fsck | Planned |
| F10 | fsck | addrs hoặc block gián tiếp trỏ ngoài vùng data | Phát hiện | corrupt+fsck | Planned |
| F11 | fsck | Thư mục tạo vòng lặp | Không treo, báo lỗi | corrupt+fsck | Planned |
| F12 | fsck | Chạy fsck -n | Ảnh không bị ghi (checksum trước và sau như nhau) | fsck | Planned |
| F13 | fsck | Thiếu tham số hoặc file không tồn tại | In usage, exit khác 0 | fsck | Planned |
| F14 | fsck | File đã unlink nhưng còn mở lúc crash | Không bị "sửa" nhầm, sau boot fsck clean | crash_loop | Planned |
| M01 | mkfs | Ảnh mới từ mkfs | /lost+found tồn tại, fsck clean | mkfs+fsck | Planned |
| T01 | Stress | crash_loop 10 lần | Không treo, có seed và điểm crash trong CSV | crash_loop | Planned |
| T02 | Stress | 200+ lần crash | Thống kê tỉ lệ fsck clean, so sánh trước và sau CRC | crash_loop | Planned |
| T03 | Stress | Chạy lại với cùng seed | Cùng kết quả | crash_loop | Planned |
| T04 | Stress | CPUS=1 và CPUS=4, mỗi loại 10 lần | Đều pass | crash_loop | Planned |
| T05 | Stress | stressfs nhiều tiến trình ghi đồng thời rồi crash | fsck clean sau reboot | crash_loop | Planned |
| R01 | Rò rỉ | Đếm free block và free inode trước và sau suite | Không đổi | suite | Planned |
| R02 | Hồi quy | usertests của xv6 gốc | ALL TESTS PASSED | usertests | Planned |
| R03 | Hồi quy | grind | Không panic | grind | Planned |
| B01 | Đo đạc | Throughput ghi trước và sau CRC (>=5 lần lặp) | Báo cáo trung bình và độ lệch | bench | Planned |
| B02 | Đo đạc | Thời gian fsck theo kích thước ảnh | Bảng và đồ thị | bench | Planned |

## W04 – Kết quả kiểm thử N10 (Đàm Quang Huy)

### Automated filesystem tests

Lệnh: make test

Kết quả: 15 passed, 0 failed.

- F01: Clean image – PASS
- F02a: Bad superblock magic – PASS
- F02b: Bad superblock size – PASS
- F03: Bitmap clear detection – PASS
- F04: Bitmap set detection – PASS
- F05: Nlink mismatch detection – PASS
- F06: Duplicate block detection – PASS
- F07: Dirent drop detection – PASS
- F12: Read-only image unchanged – PASS
- F13a: Missing arguments – PASS
- F13b: Invalid option – PASS
- F13c: Missing image – PASS
- Y01: Bitmap clear repair – PASS
- Y02: Bitmap set repair – PASS
- Y03: Nlink repair – PASS

### Crash-loop tests

- CPUS=1, seeds 211–220: 10/10 PASS.
- CPUS=4, seeds 311–320: 10/10 PASS.
- CPUS=1, NMAX=6, seeds 411–420: 10/10 PASS.
- CPUS=4, NMAX=6, seeds 511–520: 10/10 PASS.
- POINT=3, COUNT=4: 1/1 PASS.

Tổng: 40/40 lượt crash-loop PASS và
1 lượt thử riêng POINT=3, COUNT=4 PASS.

Mỗi lượt kiểm tra crash, reboot và fsck -n clean.

### Evidence

- docs/bao-cao/W04/huy/n10-make-test.log
- docs/bao-cao/W04/huy/crash_runs_w04_nwide_cpus1.csv
- docs/bao-cao/W04/huy/crash_runs_w04_nwide_cpus4.csv

### Limitations

- Chưa đạt 200+ lượt crash stress.
- Chưa bao phủ mọi tổ hợp point và N.
- Chưa hoàn thành toàn bộ test log corruption.
- CI cần được xác nhận sau khi push.

## W05 – Kết quả kiểm thử N14 (Đàm Quang Huy)

Ngày kiểm thử: 22/11/2026.

### 1. Automated test integration

Lệnh: `make test`

Kết quả: 25 passed, 0 failed; exit code 0.

- Filesystem tests: 19/19 PASS.
- Log recovery tests: 6/6 PASS (CPUS=1).

### 2. Các ca kiểm thử bổ sung trong W05

| ID | Nội dung | Kết quả |
|---|---|---|
| Y04 | Phục hồi orphan vào /lost+found | PASS |
| Y05 | Từ chối sửa duplicate block | PASS |
| Y06 | Từ chối sửa thư mục thiếu entry "." | PASS |
| Y07 | Kiểm tra fsck -y idempotent | PASS |

### 3. Log recovery integration

- L00: Normal recovery – PASS.
- L01: CRC mismatch – PASS.
- L02: Invalid magic – PASS.
- L03: Invalid log n – PASS.
- L04: Invalid crash_ctl arguments – PASS.
- L05: Negative log n – PASS.

### 4. Evidence

- docs/bao-cao/W05/huy/n14-make-test.log

### 5. Limitations

- Log recovery mới được chạy trong make test với CPUS=1.
- Chưa xác nhận CI trên PR W05.
- Chưa hoàn thành toàn bộ test matrix và stress 200+ lần.
