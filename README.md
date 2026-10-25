# Đề tài: Crash Consistency và fsck trên Hệ điều hành xv6-riscv

Dự án mở rộng hệ thống file (File System) của kernel **xv6-riscv** nhằm giải quyết bài toán **nhất quán dữ liệu khi sập nguồn đột ngột (Crash Consistency)** và xây dựng công cụ **kiểm tra, sửa chữa hệ thống file (`fsck`)**.

---

## 1. Mục tiêu & Yêu cầu kỹ thuật

1. **Checksum CRC32 cho Log Commits (`kernel/crc32.c`, `kernel/log.c`):**
   - Mở rộng `struct logheader` để lưu mã kiểm tra `crc32` tính trên toàn bộ header và các khối dữ liệu (data blocks) trong một giao dịch (transaction).
   - Khi khởi động lại (`recover_from_log`), kernel tính lại CRC32. Nếu checksum sai (do crash giữa lúc đang ghi log), giao dịch bị hủy bỏ an toàn, không chép đè dữ liệu rác.

2. **Công cụ kiểm tra và sửa lỗi `fsck` (`fsck/fsck.c`):**
   - Kiểm tra và tự động sửa (repair) các bất nhất trên ảnh đĩa `fs.img`:
     - **Bitmap:** Đối chiếu các block đang được dùng bởi inode và trạng thái bit trong bitmap.
     - **Link Counts (`nlink`):** Đếm lại số tham chiếu thực tế tới từng inode từ cây thư mục và sửa lại `nlink` hoặc thu hồi inode mồ côi.
     - **Directories:** Kiểm tra tính hợp lệ của `.` và `..`, cấu trúc thư mục.

3. **Cơ chế Tiêm lỗi - Crash-Injection Switch (`kernel/crash.c`):**
   - Bổ sung syscall và các điểm gãy có điều kiện trong `log.c` và `virtio_disk.c` để chủ động đánh sập QEMU giữa chừng khi đang commit hoặc ghi đĩa.

4. **Kịch bản Kiểm thử tự động (`scripts/`):**
   - **200+ Crash Runs (`crash_loop.sh`):** Chạy tải -> Crash QEMU giữa lúc commit -> Reboot -> `fsck` báo clean.
   - **Corrupted-image Tests (`corrupt.c` & `test_corrupt.sh`):** Chủ động làm hỏng `fs.img` và kiểm tra khả năng khôi phục của `fsck`.

5. **Mục tiêu mở rộng (Stretch Goals):**
   - **Group Commit** và syscall **`fsync()`**.

---

## 2. Cấu trúc thư mục

```text
xv6-26121/
├── README.md                 # Tài liệu tổng quan và hướng dẫn
├── Makefile                  # Build Kernel, User programs và Host tools
├── CMakeLists.txt            # Cấu hình hỗ trợ IDE (CLion / VS Code IntelliSense)
├── kernel/                   # Mã nguồn Kernel xv6-riscv
│   ├── log.c                 # [Sửa đổi] Tích hợp CRC32 commit, recovery & group commit
│   ├── crc32.c / crc32.h     # [Bổ sung] Thuật toán tính mã kiểm tra CRC32
│   ├── crash.c / crash.h     # [Bổ sung] Quản lý trạng thái và điểm tiêm lỗi (crash points)
│   ├── virtio_disk.c         # [Sửa đổi] Hook giả lập mất điện tại tầng đĩa
│   ├── sysfile.c             # [Sửa đổi] Syscall sys_crash_ctl và sys_fsync
│   └── fs.h                  # [Sửa đổi] Cập nhật struct logheader
├── user/
│   ├── crashtest.c           # [Bổ sung] Chương trình sinh tải và kích hoạt điểm crash
│   └── ...
├── mkfs/
│   └── mkfs.c                # Công cụ tạo fs.img gốc
├── fsck/                     # [Bổ sung] Công cụ kiểm tra & làm hỏng ảnh đĩa trên Host
│   ├── fsck.c
│   └── corrupt.c
├── scripts/                  # [Bổ sung] Script tự động hóa 200+ lần crash & test repair
│   ├── crash_loop.sh
│   └── test_corrupt.sh
├── docs/                     # Tài liệu thiết kế và báo cáo
└── results/                  # Lưu trữ log thực nghiệm
