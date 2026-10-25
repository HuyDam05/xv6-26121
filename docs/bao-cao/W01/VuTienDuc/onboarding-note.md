# Ghi chú Onboarding Tuần 1 - Vũ Tiến Đức (Vai B: fsck)

## 1. Bố cục đĩa của xv6 (`kernel/fs.h`, `mkfs/mkfs.c`)
Thứ tự các vùng trên ảnh đĩa `fs.img`:
`[ Boot block (0) | Superblock (1) | Log blocks | Inode blocks | Bitmap block | Data blocks ]`

- `struct superblock`: Lưu tổng số block (`size`), số data block (`nblocks`), số inode (`ninodes`), số log block (`nlog`), và block bắt đầu của từng vùng.
- `struct dinode`: Gồm `type` (T_DIR, T_FILE, T_DEVICE, hoặc 0 nếu trống), `nlink`, `size`, và mảng `addrs[NDIRECT+1]`.

## 2. Bảng 5 loại lỗi hệ thống file cần phát hiện và sửa chữa trong `fsck`

| Loại lỗi | Cách phát hiện | Cách sửa (Repair) |
|---|---|---|
| **Bitmap sai** | Quét mọi inode đang dùng, tập hợp các block dữ liệu thực tế rồi so với bitmap trên đĩa | Đặt lại bit tương ứng trong bitmap block |
| **`nlink` sai** | Duyệt toàn bộ cây thư mục đếm số `dirent` thực sự trỏ tới từng inode | Ghi đè lại giá trị `dinode.nlink` đúng thực tế |
| **Block trùng** | Một địa chỉ data block xuất hiện ở nhiều hơn một inode | Báo lỗi cụ thể ra màn hình kiểm tra |
| **Inode mồ côi** | Inode có `type != 0` nhưng số lượng `dirent` trỏ tới bằng 0 | Tạo mục liên kết đưa vào thư mục `/lost+found` |
| **Thư mục hỏng** | Thiếu mục `.` hoặc `..`, hoặc `dirent` trỏ tới inode ngoài phạm vi | Xóa `dirent` hỏng hoặc khôi phục liên kết `.`/`..` |

## 3. Quyết định kỹ thuật
- Viết `fsck` bằng C chạy trực tiếp trên máy host, `#include "kernel/fs.h"` để đọc thẳng file `fs.img`.
- Cần cập nhật `mkfs/mkfs.c` để tạo sẵn thư mục `/lost+found` từ lúc khởi tạo ảnh đĩa.
