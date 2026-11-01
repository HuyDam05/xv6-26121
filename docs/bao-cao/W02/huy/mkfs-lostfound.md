# mkfs tạo /lost+found - Đàm Quang Huy (tuần 2)

## 1. Mục tiêu
Đề tài cần thư mục /lost+found trong ảnh đĩa để fsck -y đưa các inode mồ côi
(inode còn được cấp phát nhưng không có dirent nào trỏ tới) vào đó. Trước thay đổi,
mkfs chỉ tạo thư mục gốc, chưa có /lost+found.

## 2. Thay đổi
- File sửa: mkfs/mkfs.c (khoảng 25 dòng thêm).
- mkfs cấp một inode T_DIR bằng ialloc, ghi hai dirent "." (trỏ về chính nó) và ".."
  (trỏ về thư mục gốc), thêm dirent "lost+found" vào thư mục gốc.
- Quy ước nlink (theo sys_mkdir của xv6):
  - nlink của lost+found là 1 (từ dirent "lost+found" trong thư mục gốc, ialloc đặt sẵn 1).
  - "." không tính vào nlink để tránh vòng đếm.
  - ".." trỏ về thư mục gốc nên nlink của thư mục gốc tăng thêm 1.

## 3. Cách kiểm tra
```
rm -f fs.img mkfs/mkfs && make fs.img fsck/fsck
./fsck/fsck -n fs.img
~/run-xv6.sh      # trong xv6: ls, ls lost+found, usertests -q
```

## 4. Kết quả
- fsck -n trên ảnh mới: fsck: clean, exit = 0
  (log: mkfs-lostfound-fsck.log)
- ls: có xuất hiện lost+found 1 2 32
- ls lost+found: . 1 2 32
                 .. 1 1 1024  
- usertests -q (CPUS=3): ALL TESTS PASSED 
  (log: mkfs-lostfound-xv6.log)

## 5. Hạn chế và việc tiếp theo
- Số inode của các file do mkfs tạo dịch lên 1 so với trước (bình thường).
- fsck hiện chưa dùng /lost+found; chức năng đưa inode mồ côi vào đây thuộc fsck -y (tuần sau).
- mkfs là công cụ chạy trên host, nên thay đổi này không chạm vào kernel.
- <điều khác bạn quan sát được, nếu có>