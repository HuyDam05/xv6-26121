## 2. Thiết kế Log và CRC (Vũ Khắc Hùng)

### 2.1 Cấu trúc dữ liệu

`struct logheader` nằm ở block đầu của vùng log (block 2). Các block log dữ liệu nằm sau nó (block 3 đến 32).

| Trường | Kích thước | Ý nghĩa |
|---|---|---|
| `n` | 4 B | Số block trong transaction |
| `block[LOGBLOCKS]` | 30 × 4 = 120 B | Số block đích của từng block log |
| `magic` | 4 B | `LOG_MAGIC = 0x4C4F4743` ("LOGC"), nhận biết header hợp lệ |
| `crc` | 4 B | CRC32 của transaction |

Tổng 132 B, nhỏ hơn `BSIZE` = 1024 B (`initlog` gọi `panic` nếu không thỏa). CRC dùng đa thức 0xEDB88320 và được kiểm thử trên host (PR #11). CRC tính trên header (`n` và danh sách `block[]`) cùng nội dung `n` block log trên đĩa. Nội dung log block nằm trong phạm vi CRC đã được kiểm chứng ở mục 2.5.

### 2.2 Thứ tự commit và 5 điểm crash

```
begin_op ... end_op -> commit():
  1. write_log()      chép block từ cache vào vùng log
     [điểm 1] COMMIT_BEFORE_HDR   (log đã xuống đĩa, header chưa ghi)
  2. write_head()     ghi header (n, magic, crc) = COMMIT POINT
     [điểm 2] COMMIT_AFTER_HDR    (đã commit, chưa install)
  3. install_trans(0) chép từng block về vị trí thật
     [điểm 3] INSTALL_MID         (sau mỗi block, không gọi lúc recovery)
     [điểm 5] INSTALL_DONE        (install xong, header vẫn n > 0)
  4. n = 0, crc = 0, write_head()  xóa transaction
```

Điểm 4 `DISK_WRITE` nằm trong `bwrite` (kernel/bio.c): crash ở lần ghi block thứ N.

### 2.3 Bảng quyết định recovery (`recover_from_log`)

| Điều kiện | Hành động | Thông báo |
|---|---|---|
| `n < 0` hoặc `n > LOGBLOCKS` | Bỏ log, không đọc `block[]` | `log: bad header n=...` |
| `n = 0` | Không làm gì | — |
| `n > 0`, magic sai | Bỏ log | `log: bad magic, dropping log` |
| `n > 0`, magic đúng, CRC lệch | Bỏ log | `log: crc mismatch, dropping log` |
| `n > 0`, magic đúng, CRC khớp | Replay (`install_trans(1)`) | `recovering tail i dst b` |

Sau mọi trường hợp, kernel ghi lại header với `n = 0`, `crc = 0`.

### 2.4 Syscall `crash_ctl(point, count)`

Số syscall 23. `point` hợp lệ trong khoảng 0 đến `CRASH_MAXPOINT` (= 5), ngoài khoảng đó trả lỗi `invalid argument` và không crash. `count` là lần gọi thứ N của đúng điểm đó. Chương trình `crashtest <point> <count>` đặt điểm crash rồi để workload chạy tiếp.

### 2.5 Kết quả thực nghiệm

**Crash tại từng điểm** (QEMU, `crashtest <điểm> 1`, sau đó boot lại trên cùng `fs.img`):

| Điểm | Khi boot lại | File tạo dở (`ct0`) | `fsck -n` |
|---|---|---|---|
| 1 `COMMIT_BEFORE_HDR` | Không có `recovering tail`, log bị bỏ | Không có | clean |
| 2 `COMMIT_AFTER_HDR` | `recovering tail 0..2` (dst 34, 47, 33) | Có | clean |
| 3 `INSTALL_MID` | `recovering tail 0..2` | Có | clean |
| 5 `INSTALL_DONE` | `recovering tail 0..2` | Có | clean |

Crash trước commit point thì transaction biến mất hoàn toàn, sau commit point thì được replay đủ. Ở điểm 3 và 5, block đã install bị ghi lại lần hai mà dữ liệu vẫn đúng, nên replay là idempotent.

**Test log hỏng (H3):** lấy ảnh đĩa sau điểm 2 (log đã commit, `n = 3`), đảo toàn bộ bit của 1 byte trong log block 0, giữ nguyên header. Boot lại in `log: crc mismatch, dropping log`, không có `recovering tail` nào, file `ct0` không xuất hiện, `fsck -n` báo clean. CRC phát hiện được lỗi trong nội dung log block dù header hợp lệ.

### 2.6 File sẽ sửa

`kernel/log.c`, `kernel/crc32.c/.h`, `kernel/crash.c/.h`, `kernel/bio.c`, `kernel/syscall.h/.c`, `kernel/sysproc.c`, `user/usys.pl`, `user/user.h`, `user/crashtest.c`.

### 2.7 Trạng thái và việc còn lại

Đã hoàn thành: 5 điểm crash và syscall `crash_ctl`; kiểm chứng recovery tại các điểm 1, 2, 3, 5; kiểm chứng CRC bắt log hỏng (nhánh CRC lệch của bảng 2.3).

Tuần 4: kiểm chứng hai nhánh còn lại của bảng 2.3 (magic sai, `n` hỏng); tự động hóa các ca log hỏng trong `make test` khi công cụ `corrupt` có các mode `log-*`; chạy lặp 200+ lần crash có seed trước và sau CRC để so sánh.
