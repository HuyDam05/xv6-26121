# Thiết kế CRC32 cho log (N01 - Vũ Khắc Hùng)

Base: kernel/log.c của xv6-riscv (commit 06aad25). Hàm tính: `crc32()` trong kernel/crc32.c (đa thức 0xEDB88320).

## 1. Mục tiêu
Header log hiện chỉ có `n` và `block[]`. Nếu log ghi dở hoặc bị hỏng nhưng header đã ghi, `recover_from_log` vẫn replay dữ liệu rác về vị trí thật. CRC giúp recovery phát hiện và bỏ transaction hỏng.

## 2. CRC tính trên cái gì
Một CRC duy nhất cho cả transaction, tính nối tiếp theo thứ tự cố định:
1. Trường `n` (4 byte).
2. Mảng `block[0..n-1]` (danh sách block đích, 4 x n byte).
3. Nội dung `n` log block, theo thứ tự `tail = 0..n-1`, mỗi block BSIZE (1024) byte.

Không tính trường `crc` (chính nó) và trường `magic`. `crc32()` hỗ trợ nối tiếp nên viết: `c = crc32(0, &n, 4); c = crc32(c, block, 4*n); c = crc32(c, data_i, BSIZE)` cho từng block.

## 3. Tính ở đâu và tính trên dữ liệu nào
- Tính trong `write_log()`, ngay trong vòng lặp ghi từng log block.
- Dữ liệu đưa vào CRC là **bản đã chép vào log block** (`to->data` sau `memmove`), chính là thứ được `bwrite` xuống đĩa. Không tính trên block trong buffer cache.
- Lý do: `begin_op` chặn thao tác mới khi đang commit (`log.committing`), nên `log.lh.n` và `log.lh.block[]` không đổi trong lúc commit; còn bản sao trong log block là dữ liệu cố định cuối cùng sẽ được replay.
- Kết quả lưu vào `log.lh.crc`, và `write_head()` (lần 1, commit point) ghi `n`, `block[]`, `magic`, `crc` xuống đĩa trong cùng một block header.

## 4. Thay đổi `struct logheader`

    struct logheader {
      int n;
      int block[LOGBLOCKS];
      uint magic;   // LOG_MAGIC, nhận diện header hợp lệ
      uint crc;     // CRC32 của transaction
    };

`LOG_MAGIC = 0x4C4F4743` (chữ "LOGC"). Kích thước: 4 + 4 x 30 + 8 = 132 byte, nhỏ hơn BSIZE = 1024. `initlog` đã có kiểm tra `sizeof(struct logheader) >= BSIZE`.

## 5. Kiểm tra khi recovery (`recover_from_log`)
1. `read_head()` đọc header (cần đọc thêm `magic` và `crc`).
2. Nếu `n == 0`: không có gì để replay, bỏ qua kiểm tra CRC (xử lý cả ảnh đĩa mới tạo bởi mkfs, header toàn 0).
3. Nếu `n < 0` hoặc `n > LOGBLOCKS` hoặc `magic != LOG_MAGIC`: coi header hỏng, đặt `n = 0`, không replay (tránh đọc ngoài mảng `block[]`).
4. Ngược lại: đọc lại `n` log block từ đĩa, tính CRC cùng thứ tự như mục 2, so với `crc` trong header.
   - Khớp: `install_trans(1)`, replay bình thường.
   - Lệch: coi như chưa commit, không `install_trans`, in cảnh báo.
5. Cuối cùng đặt `n = 0` và `write_head()` như cũ để xóa log.

## 6. Header lần 2 (xóa log)
`commit()` ghi header lần 2 với `n = 0`. Đặt `crc = 0` để không để CRC cũ nằm lại trên đĩa; `magic` giữ nguyên hoặc ghi lại giá trị hợp lệ.

## 7. Phương án đã cân nhắc
- CRC riêng từng block: phát hiện được block nào hỏng nhưng cần chỗ lưu thêm 4 byte x 30 và so sánh nhiều lần; không cần thiết vì khi một block hỏng thì cả transaction cũng bị bỏ.
- Một CRC cho cả transaction (chọn): đơn giản, vừa trong header, khớp với tính nguyên tử của commit point (một lần ghi header).

## 8. Kiểm thử dự kiến
- Test vector: crc32("123456789") = 0xCBF43926 (đã có trong tests/crc32_host_test.c).
- Chạy bình thường (công tắc crash tắt): `usertests -q` vẫn ALL TESTS PASSED ở CPUS=1 và CPUS=4.
- Làm hỏng một log block khi header còn n > 0 (bằng fsck/corrupt): boot lại, recovery phải bỏ transaction và in cảnh báo.
- Crash trước và sau commit point (crash-injection): trước thì không replay, sau thì replay đủ.

## 9. Lưu ý
Crash bằng cách thoát QEMU giữa hai lần ghi block không tạo ra torn write hay ghi sai thứ tự, nên vòng lặp crash thông thường khó làm CRC lệch. Test làm hỏng log block ở mục 8 là cách chứng minh CRC hoạt động.
