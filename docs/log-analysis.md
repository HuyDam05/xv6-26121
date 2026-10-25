# Phân tích kernel/log.c (K01 – Tuần 1)

Người viết: Vũ Khắc Hùng (20233428) · Đề tài 26121 · Liên quan backlog: E01, E02, L01, L02

> Base commit: `06aad25` (Add killzero test). Chưa có sửa đổi nào trong `kernel/log.c`.
> Tham số (kernel/param.h): `MAXOPBLOCKS = 10`, `LOGBLOCKS = 30` (vùng log trên đĩa gồm 1 block header + 30 block dữ liệu = 31 block).

## 1. Các hàm chính

| Hàm | Vai trò |
|---|---|
| `begin_op` | Đánh dấu bắt đầu một system call FS; ngủ chờ nếu đang commit hoặc log có nguy cơ hết chỗ |
| `log_write` | Ghi nhớ số block bị sửa vào `log.lh.block[]` (chưa ghi đĩa); có log absorption |
| `end_op` | Giảm `outstanding`; nếu về 0 thì gọi `commit()` |
| `commit` | Thực hiện 4 bước ở mục 2 |
| `write_log` | Chép các block đã sửa từ cache vào vùng log (`log.start + tail + 1`) |
| `write_head` / `read_head` | Ghi / đọc header (`n`, `block[]`) xuống / từ đĩa |
| `install_trans` | Chép block từ vùng log về vị trí thật |
| `recover_from_log` | Lúc boot: `read_head`, `install_trans(1)`, `n = 0`, `write_head` |
| `sys_sync` | Chờ commit hiện tại hoàn tất (liên quan stretch `fsync`) |

## 2. Luồng commit và commit point

    commit():
      write_log()        // (1) chép block đã sửa vào vùng log
      write_head()       // (2) COMMIT POINT: header n > 0 được ghi xuống đĩa
      install_trans(0)   // (3) chép về vị trí thật
      log.lh.n = 0
      write_head()       // (4) xóa header, kết thúc transaction

**Commit point** là lần `write_head()` đầu tiên (bước 2). Trước nó, transaction coi như chưa xảy ra; sau nó, transaction chắc chắn được hoàn tất khi recovery.

Luồng recovery lúc boot (`initlog` gọi `recover_from_log`): `read_head`, nếu `n > 0` thì `install_trans(1)`, rồi `n = 0` và `write_head`.

## 3. Trạng thái đĩa nếu crash tại từng điểm

| Crash tại | Trạng thái đĩa | Recovery |
|---|---|---|
| Trong `write_log` (trước `write_head`) | Log ghi dở, header vẫn `n = 0` | Không replay; transaction bị bỏ |
| Ngay sau `write_head` đầu | Log đủ, header `n > 0` | Replay, transaction hoàn tất |
| Giữa `install_trans` | Một phần block đã về chỗ thật | Replay lại từ đầu (idempotent) |
| Sau `install_trans`, trước `write_head(n=0)` | Mọi block đã về chỗ thật | Replay thêm lần nữa, vô hại |
| Sau `write_head(n=0)` | Hoàn tất | Không làm gì |

## 4. Thiết kế `struct logheader` mới (đề xuất)

    struct logheader {
      int n;
      int block[LOGBLOCKS];
      uint magic;   // nhận diện header hợp lệ
      uint crc;     // CRC32 (đa thức 0xEDB88320)
    };

Kích thước hiện tại: 4 + 4 x 30 = 124 byte. Sau khi thêm `magic` và `crc`: 132 byte, nhỏ hơn `BSIZE = 1024`. `initlog` đã có kiểm tra `sizeof(struct logheader) >= BSIZE`.

Phạm vi tính CRC: trường `n`, mảng `block[0..n-1]` và nội dung `n` log block; **không** gồm chính trường `crc`.

Các hàm phải sửa đồng bộ:
1. `write_log`: tính CRC dồn qua dữ liệu từng log block đang ghi, lưu vào `log.lh.crc`.
2. `write_head` và `read_head`: bổ sung chép `magic` và `crc`.
3. `recover_from_log`: nếu `n > 0` thì đọc lại các log block, tính CRC và so sánh. Lệch thì coi như chưa commit (đặt `n = 0`, **không** gọi `install_trans`).

Các điểm cần lưu ý:
- **Header toàn số 0**: `mkfs` ghi header toàn 0 (`n = 0`, `magic = 0`). Nếu `n == 0` thì bỏ qua kiểm tra magic/CRC, tránh coi đĩa mới tạo là hỏng.
- **Lần `write_head` thứ hai** (xóa header) phải ghi nhất quán các trường mới, không để `crc` cũ nằm lại trên đĩa.
- `struct logheader` hiện khai báo trong `log.c`; nếu `fsck` trên host cần đọc header thì phải chuyển khai báo sang header dùng chung (liên quan G02).

## 5. Vị trí đặt crash-injection

| # | Vị trí trong code | Ý nghĩa |
|---|---|---|
| 1 | Trong vòng lặp `write_log`, sau block thứ k | Log ghi dở |
| 2 | Cuối `write_log`, ngay trước `write_head()` | Log đủ nhưng chưa commit |
| 3 | Ngay sau `write_head()` đầu tiên | Vừa qua commit point |
| 4 | Trong vòng lặp `install_trans`, sau block thứ k | Install dở (lúc bitmap/inode thật sự được ghi xuống vị trí thật) |
| 5 | Sau `install_trans`, trước `write_head()` thứ hai | Install xong, chưa xóa header |
| 6 | Ngay sau `write_head()` thứ hai | Hoàn tất |

Ghi chú: mọi sửa đổi bitmap và inode đều đi qua `log_write`, nên trên đĩa thật chúng chỉ bị ghi ở giai đoạn `install_trans`. Do đó điểm 4 chính là điểm "ghi bitmap/inode". Nếu logging đúng thì sau crash và reboot, `fsck` gần như luôn báo clean; vì vậy test corrupted-image phải tự làm hỏng ảnh đĩa có chủ đích.

Cách cài: hàm `crash_point(int id)` đếm số lần gọi, đạt ngưỡng thì ghi `0x5555` vào `0x100000` để thoát QEMU; có công tắc tắt (`CRASH_NONE`) mặc định để `usertests` và `grind` không bị ảnh hưởng.

## 6. Sơ đồ Figma

- Luồng commit: (bổ sung link Figma)
- Luồng `recover_from_log` (nhánh kiểm tra CRC ghi chú "dự kiến tuần 2"): (bổ sung link Figma)

## 7. Việc tuần 2 (N01)

- Hàm `crc32` đa thức `0xEDB88320`; test trên host: `crc32("123456789") == 0xCBF43926`.
- PR cập nhật `logheader` (`magic`, `crc`).
- Khung biến đếm crash-injection, map `0x100000` bằng `kvmmap` trong `kvmmake`.
