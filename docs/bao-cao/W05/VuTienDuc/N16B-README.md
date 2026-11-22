# W05 – N16B: Orphan Recovery

**Người thực hiện:** Vũ Tiến Đức

## 1. Mục tiêu

Bổ sung khả năng phục hồi orphan inode trong xv6
thông qua công cụ fsck.

## 2. Chức năng triển khai

- Tìm inode không thể truy cập từ thư mục gốc.
- Chỉ phục hồi một orphan inode kiểu file thông thường,
  có nlink = 1.
- Gắn inode phục hồi vào /lost+found/file<inum>.
- Không cấp phát block mới khi phục hồi.
- Từ chối các trường hợp không bảo đảm điều kiện an toàn.
- Nhận diện directory entry đã ghi dở để tiếp tục
  một trường hợp phục hồi bị gián đoạn.
- Bổ sung công cụ fsck-safe-repair.py để thực hiện
  sửa chữa offline trên ảnh tạm, xác minh bằng fsck -n
  rồi mới thay thế ảnh đích.

## 3. Kiểm thử

| Trường hợp | Kết quả |
|---|---|
| Phục hồi inode 3 | PASS |
| Kiểm tra sau phục hồi bằng fsck -n | PASS |
| Bảo toàn nội dung dữ liệu inode 3 | PASS |
| Orphan nlink = 0 | PASS |
| lost+found không hợp lệ | PASS |
| lost+found hết chỗ | PASS |
| Nhiều orphan | PASS |
| Tiếp tục sau khi ghi dở directory entry | PASS |
| Regression | 15 passed, 0 failed |
| Safe repair từ chối ảnh không sửa được | PASS |

## 4. Minh chứng

- orphan-repair.log
- orphan-negative.log
- orphan-regression.log

## 5. Giới hạn

- Chỉ hỗ trợ phục hồi một orphan file mỗi lần.
- Không hỗ trợ phục hồi orphan directory.
- Chỉ sử dụng block dữ liệu đầu tiên đã cấp phát
  của /lost+found.
- Không bảo đảm tính nguyên tử cho thao tác ghi trực tiếp
  bằng fsck -y.
- Công cụ safe repair chỉ dành cho ảnh đĩa offline,
  không được sử dụng đồng thời với QEMU.
- Các bài kiểm thử hiện tại chưa bao phủ mọi dạng
  hỏng filesystem hoặc mọi thời điểm mất điện.
