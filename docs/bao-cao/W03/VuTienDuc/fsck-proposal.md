# Thiết kế công cụ fsck – Tuần 3
## Vũ Tiến Đức – Vai B

### 1. Mục tiêu

Xây dựng công cụ fsck chạy trên host Ubuntu, đọc trực tiếp ảnh đĩa fs.img của xv6-riscv để phát hiện các lỗi nhất quán hệ thống file sau crash.

### 2. Quy trình kiểm tra

1. Đọc superblock, kiểm tra FSMAGIC và kích thước filesystem.
2. Duyệt inode, kiểm tra direct/indirect block.
3. Sử dụng used_blocks[] để phát hiện block trùng và đối chiếu bitmap.
4. Duyệt các thư mục và kiểm tra dirent, dấu . và ..
5. Đếm số liên kết tới từng inode và so sánh với dinode.nlink.
6. Phát hiện inode mồ côi và kiểm tra khả năng truy cập từ thư mục gốc.

### 3. Cấu trúc dữ liệu

- `used_blocks[]`: đánh dấu các block đang được sử dụng để kiểm tra block trùng và bitmap.
- `links[]`: đếm số liên kết thực tế tới từng inode để đối chiếu với `dinode.nlink`.
- `reachable[]`: đánh dấu các inode có thể truy cập từ thư mục gốc trong quá trình BFS.
- `queue[]`: hàng đợi dùng để duyệt BFS khi phát hiện inode mồ côi.
- `parent[]`: lưu quan hệ cha của thư mục và hỗ trợ phát hiện vòng lặp.
- `dotdot[]`, `dots[]`, `dotdots[]`: kiểm tra tính hợp lệ và số lượng entry `.` và `..`.

### 4. Chế độ hoạt động

Chế độ -n: chỉ kiểm tra, không thay đổi fs.img.

Chế độ -y: dự kiến sửa lỗi tự động khi an toàn, ưu tiên sửa bitmap trước. Việc cài đặt có thể thực hiện trong Tuần 4.

Đối với inode mồ côi có thể phục hồi, dự kiến đưa vào /lost+found.

Không tự động sửa block trùng hoặc thư mục hỏng nếu chưa xác định được cách khôi phục an toàn.

Cần phân biệt inode có nlink = 0 đang chờ thu hồi sau crash với inode mồ côi thực sự.

### 5. Kế hoạch kiểm thử

- fs.img bình thường phải báo clean.
- Ảnh lỗi bitmap phải phát hiện bitmap mismatch.
- Ảnh sửa nlink phải phát hiện nlink mismatch.
- Ảnh lỗi dirent phải phát hiện cấu trúc thư mục sai.
- Ảnh inode mồ côi phải được phân loại chính xác.
- Chế độ -n không được sửa nội dung ảnh đĩa.
- Sau khi -y sửa lỗi, chạy lại -n để kiểm tra kết quả.

### 6. Các file liên quan

- `fsck/fsck.c`: chương trình kiểm tra filesystem trên host Ubuntu.
- `fsck/corrupt.c`: khung công cụ hỗ trợ tạo ảnh đĩa lỗi; hiện chưa tự động thực hiện đầy đủ các chế độ corrupt.
- `kernel/fs.h`: định nghĩa các cấu trúc dữ liệu và hằng số của filesystem xv6.
- `mkfs/mkfs.c`: chương trình tạo `fs.img`, bao gồm phần khởi tạo `/lost+found`.
- `docs/bao-cao/W03/VuTienDuc/README.md`: báo cáo triển khai và kết quả Tuần 3.
- `docs/bao-cao/W03/VuTienDuc/test-results.log`: log kiểm thử và minh chứng SHA-256.

Phần thiết kế này có thể được dùng làm cơ sở để tích hợp vào tài liệu thiết kế chung của nhóm.
