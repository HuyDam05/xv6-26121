# Kế hoạch và Backlog Dự án - Nhóm 26121

## 1. Danh sách tính năng (Product Backlog)
- [ ] Task 1.1: Bảng tra cứu CRC32 trong kernel (`kernel/crc32.c`)
- [ ] Task 1.2: Checksum CRC32 cho transaction commit (`kernel/log.c`)
- [ ] Task 1.3: Kiểm tra CRC32 khi recovery tại boot time
- [ ] Task 2.1: Crash-injection points (`kernel/crash.c`, `virtio_disk.c`)
- [ ] Task 2.2: Syscall `sys_crash_ctl` và chương trình `crashtest`
- [ ] Task 3.1: Host fsck - Kiểm tra tính nhất quán của Bitmap
- [ ] Task 3.2: Host fsck - Kiểm tra và sửa Link Counts (`nlink`)
- [ ] Task 3.3: Host fsck - Kiểm tra thư mục (`.` và `..`)
- [ ] Task 4.1: Script chạy tự động 200+ lần crash QEMU
