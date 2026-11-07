# Báo cáo Tuần 3 - Vũ Tiến Đức

## 1. Mục tiêu

Tuần 3 tập trung mở rộng chương trình `fsck` đã xây dựng từ Tuần 2 để kiểm tra sâu hơn tính nhất quán của hệ thống file xv6.

Các yêu cầu chính:

- Kiểm tra số liên kết `nlink` của inode.
- Duyệt directory entry để đếm số liên kết thực tế.
- Kiểm tra các entry đặc biệt `.` và `..`.
- Phát hiện directory entry tham chiếu inode không hợp lệ.
- Phát hiện vòng lặp trong cấu trúc thư mục.
- Phát hiện và phân loại inode mồ côi.
- Thiết kế phương án sửa lỗi bằng `fsck -y`.
- Giữ nguyên chế độ chỉ kiểm tra `fsck -n`.
- Kiểm thử trên ảnh đĩa bình thường và ảnh đĩa bị cố tình làm hỏng.

---

## 2. Cấu hình hệ thống file

Tiếp tục sử dụng cấu trúc filesystem xv6 từ Tuần 2.

Các thông số:

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
- Root inode: `ROOTINO = 1`
- Direct block: 12
- Directory entry: 16 bytes

Bố cục:

    boot | superblock | log | inode | bitmap | data
      0        1       2-32   33-45     46     47-1999

Các cấu trúc quan trọng:

- `struct dinode`: lưu type, nlink, size và các địa chỉ block.
- `struct dirent`: lưu inum và tên file/thư mục.
- `IBLOCK`: xác định block chứa inode.
- `NDIRECT`: số direct block của inode.

---

## 3. Nội dung triển khai

### 3.1. D1 - Kiểm tra số liên kết nlink

Bổ sung hàm `check_nlink()` vào `fsck/fsck.c`.

Nguyên lý:

1. Duyệt các inode đã cấp phát.
2. Xác định inode thuộc loại thư mục.
3. Đọc từng directory entry trong thư mục.
4. Đếm số liên kết thực tế tới các inode.
5. So sánh số liên kết với trường `dinode.nlink`.

Quy tắc:

- Entry `.` không tính vào nlink.
- Entry `..` tính vào nlink của thư mục cha.
- Bỏ qua entry có `inum = 0`.
- Kiểm tra inode được tham chiếu có hợp lệ hay không.

Ví dụ khi phát hiện lỗi:

    ERROR: inode 1 nlink mismatch (disk=2, expected=1)

### 3.2. D2 - Kiểm tra cấu trúc thư mục

Bổ sung hàm `check_directories()`.

Các nội dung kiểm tra:

- Mỗi thư mục có đúng một entry `.`.
- Entry `.` phải trỏ về chính inode của thư mục.
- Mỗi thư mục có đúng một entry `..`.
- Entry `..` phải tham chiếu thư mục cha hợp lệ.
- Entry `..` của root phải trỏ về ROOTINO.
- Dirent không được tham chiếu inode ngoài phạm vi.
- Dirent không được tham chiếu inode chưa cấp phát.
- Phát hiện thư mục có nhiều liên kết cha.
- Phát hiện vòng lặp trong quan hệ cha - con.

Thuật toán sử dụng bảng `parent[]` để lưu quan hệ cha.

Trong quá trình kiểm thử, đã sửa điều kiện dừng của thuật toán để phát hiện cả vòng lặp đi qua root.

Ví dụ:

    ERROR: directory 1 has invalid dot entry

    ERROR: root directory has invalid dotdot

    ERROR: directory cycle detected at inode 1
    ERROR: directory cycle detected at inode 2

### 3.3. D3 - Phát hiện inode mồ côi

Bổ sung hàm `check_orphans()`.

Chương trình sử dụng thuật toán BFS để duyệt các inode có thể truy cập từ `ROOTINO`.

Cấu trúc dữ liệu:

- `reachable[]`: đánh dấu inode có thể truy cập từ root.
- `queue[]`: hàng đợi phục vụ BFS.

Quy trình:

1. Bắt đầu từ inode gốc.
2. Duyệt các directory entry.
3. Đánh dấu inode có thể truy cập.
4. Tiếp tục duyệt các thư mục con.
5. Kiểm tra inode đã cấp phát nhưng không thể truy cập từ root.

Phân loại:

**Trường hợp 1: nlink > 0**

Inode có số liên kết dương nhưng không thể truy cập từ root.

Chương trình báo lỗi:

    ERROR: orphan inode 2: nlink=1, unreachable from root

**Trường hợp 2: nlink = 0**

Inode không thể truy cập và có nlink bằng 0.

Đây có thể là inode đang chờ thu hồi sau crash.

Chương trình đưa ra cảnh báo:

    WARNING: inode 2 has nlink=0 and is unreachable (possibly pending reclamation)

Theo chính sách hiện tại, trường hợp này trả exit code 0.

---

## 4. Kiểm thử chương trình

### 4.1. Biên dịch

Lệnh:

    make fsck/fsck

Kết quả: biên dịch thành công với GCC `-Wall` và `-Werror`.

### 4.2. Kiểm tra ảnh đĩa bình thường

Lệnh:

    ./fsck/fsck -n fs.img

Kết quả:

    fsck: clean
    Exit code: 0

### 4.3. Kiểm tra ảnh đĩa bị lỗi

| STT | Ảnh đĩa | Lỗi được kiểm tra | Exit code |
|---|---|---|---|
| 1 | `fs.img` | Ảnh bình thường | 0 |
| 2 | `fs-nlink-corrupt.img` | Sai nlink | 1 |
| 3 | `fs-dir-corrupt.img` | Entry `.` không hợp lệ | 1 |
| 4 | `fs-dotdot-corrupt.img` | Entry `..` không hợp lệ | 1 |
| 5 | `fs-dirent-corrupt.img` | Dirent tham chiếu inode 999 | 1 |
| 6 | `fs-cycle-corrupt.img` | Vòng lặp thư mục | 1 |
| 7 | `fs-orphan-corrupt.img` | Orphan inode có nlink dương | 1 |
| 8 | `fs-nlink-zero.img` | Inode có nlink bằng 0 | 0 |

Các trường hợp lỗi được tạo bằng cách sao chép `fs.img` rồi chỉnh sửa metadata có chủ đích.

Kết quả chi tiết được lưu trong `test-results.log`.

### 4.4. Kiểm tra chế độ chỉ đọc

Sử dụng SHA-256 để so sánh checksum của hai ảnh đĩa trước và sau khi chạy `fsck -n`.

Kết quả:

    Diff exit code: 0

Hai ảnh đĩa được kiểm thử không thay đổi sau khi chương trình chạy.

---

## 5. Thiết kế chế độ sửa lỗi fsck -y

Trong Tuần 3, đã xây dựng bản đề xuất cho chế độ sửa chữa tự động.

Các hướng xử lý dự kiến:

- Sửa bitmap dựa trên danh sách block thực tế đang được sử dụng.
- Điều chỉnh nlink nếu xác định được số liên kết hợp lệ.
- Phục hồi orphan inode có thể khôi phục vào `/lost+found`.
- Không tự động sửa các lỗi block trùng hoặc quan hệ thư mục không rõ ràng.
- Chạy lại `fsck -n` sau khi sửa để xác nhận tính nhất quán.

Chức năng `fsck -y` chưa được cài đặt trong Tuần 3.

---

## 6. Kết quả đạt được

- Bổ sung kiểm tra nlink trong `fsck/fsck.c`.
- Bổ sung kiểm tra cấu trúc thư mục.
- Phát hiện vòng lặp thư mục.
- Xây dựng thuật toán BFS phát hiện orphan inode.
- Hoàn thành tám bài kiểm thử cơ bản.
- Xác nhận chế độ `fsck -n` không thay đổi ảnh đĩa.
- Hoàn thành bản thiết kế fsck và lưu log kiểm thử.

Các file liên quan:

    fsck/fsck.c

    docs/bao-cao/W03/VuTienDuc/
        README.md
        fsck-proposal.md
        test-results.log

Thông tin Git:

- Repository: `HuyDam05/xv6-26121`
- Branch: `feat/duc-w03-fsck`
- Commit: `055cbe3`
- Tác giả: `VuTienDuc`
- Đã push nhánh lên GitHub.

---
