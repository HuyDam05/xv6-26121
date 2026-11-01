# Báo cáo Tuần 2 - Vũ Tiến Đức

## 1. Mục tiêu

Tuần 2 tập trung xây dựng chương trình `fsck` chạy trên host Ubuntu để kiểm tra tính nhất quán của hệ thống file xv6.

Các yêu cầu chính:

- Đọc và kiểm tra superblock.
- Duyệt các inode đang được sử dụng.
- Kiểm tra các block direct và indirect của inode.
- Xây dựng danh sách các block thực tế đang được sử dụng.
- So sánh danh sách block với bitmap của hệ thống file.
- Chạy ở chế độ chỉ kiểm tra, không sửa ảnh đĩa.
- Kiểm tra trên cả ảnh đĩa bình thường và ảnh đĩa bị cố tình làm hỏng bitmap.

---

## 2. Cấu hình hệ thống file

Từ `kernel/fs.h` và superblock của `fs.img`:

- Block size: 1024 bytes
- FSMAGIC: `0x10203040`
- Tổng số block: 2000
- Số data block: 1953
- Số inode: 200
- Số log block: 31
- Log bắt đầu tại block 2
- Inode bắt đầu tại block 33
- Bitmap bắt đầu tại block 46
- Data bắt đầu tại block 47

Bố cục:

```text
boot | superblock | log | inode | bitmap | data
  0        1        2-32   33-45     46     47-1999
