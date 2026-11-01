# Implementation Note - fsck Tuần 2

## 1. Source code

Phần cài đặt chính nằm tại:

```text
fsck/fsck.c
## 2. Luồng xử lý

fsck thực hiện kiểm tra theo thứ tự:

1. Mở fs.img ở chế độ chỉ đọc.
2. Đọc superblock tại block 1.
3. Kiểm tra FSMAGIC và kích thước filesystem.
4. Duyệt toàn bộ inode đang sử dụng.
5. Kiểm tra direct block và indirect block.
6. Đánh dấu các block đang được sử dụng.
7. Đọc bitmap.
8. So sánh bitmap với danh sách block thực tế.
9. Báo filesystem clean hoặc inconsistency.

## 3. Kiểm tra superblock

Superblock của filesystem hiện tại có:

- FSMAGIC: 0x10203040
- Tổng số block: 2000
- Data block: 1953
- Inode: 200
- Log block: 31
- Log bắt đầu: block 2
- Inode bắt đầu: block 33
- Bitmap bắt đầu: block 46
- Data bắt đầu: block 47

fsck kiểm tra magic và kích thước filesystem trước khi tiếp tục các bước khác.

## 4. Kiểm tra inode

Chương trình duyệt inode từ 1 đến ninodes - 1.

Inode có type = 0 được coi là chưa sử dụng.

Với inode đang sử dụng, fsck kiểm tra:

- 12 direct block.
- Indirect block.
- Các block dữ liệu được tham chiếu qua indirect block.
- Block có nằm trong phạm vi hợp lệ hay không.
- Block có bị tham chiếu nhiều lần hay không.

## 5. Kiểm tra bitmap

Mảng used_blocks được sử dụng để lưu các block thực sự đang được inode tham chiếu.

Sau đó fsck đọc bitmap trên đĩa và so sánh.

Hai trường hợp lỗi:

- Block đang được sử dụng nhưng bitmap đánh dấu free.
- Bitmap đánh dấu block used nhưng không có inode nào tham chiếu.

## 6. Chế độ -n

Chương trình mở ảnh đĩa bằng chế độ rb nên chỉ đọc và không thay đổi fs.img.

Lệnh:

```bash
./fsck/fsck -n fs.img
## 7. Kiểm thử ảnh sạch

Lệnh:

```bash
./fsck/fsck -n fs.img


## 8. Kiểm thử ảnh lỗi bitmap

Tạo bản sao ảnh đĩa:

```bash
cp fs.img fs-corrupt.img
