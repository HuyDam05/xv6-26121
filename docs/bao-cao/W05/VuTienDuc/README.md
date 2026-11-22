# W05 N16A - Vũ Tiến Đức

## 1. Mục tiêu

Kiểm chứng cơ chế ireclaim của xv6-riscv sau crash/reboot,
đảm bảo thu hồi inode và data block của file đã unlink
nhưng vẫn còn file descriptor mở.

## 2. Triển khai

- Kiểm tra fsinit(), ireclaim(), iput(), itrunc(), ifree().
- Bổ sung user/ireclaimtest.c.
- Bổ sung chương trình _ireclaimtest vào Makefile.
- Kiểm thử trên CPUS=1 và CPUS=4.
- Không thay đổi kernel/fs.c do chưa phát hiện lỗi.

## 3. Kết quả kiểm thử

### CPUS=1

- Baseline: 846 free blocks, 171 free inodes.
- Sau unlink: 844 free blocks, 170 free inodes.
- Crash khi file descriptor vẫn mở.
- Reboot phát hiện orphan inode 29.
- Sau recovery: 846 free blocks, 171 free inodes.
- fsck -n: clean, exit 0.
- Kết quả: PASS.

### CPUS=4

- Baseline: 846 free blocks, 171 free inodes.
- Sau unlink: 844 free blocks, 170 free inodes.
- Crash khi file descriptor vẫn mở.
- Reboot phát hiện orphan inode 29.
- Sau recovery: 846 free blocks, 171 free inodes.
- fsck -n: clean, exit 0.
- Kết quả: PASS.

## 4. Regression

- usertests CPUS=1: ALL TESTS PASSED.
- usertests CPUS=4: ALL TESTS PASSED.
- fsck -n không thay đổi SHA-256 của ảnh đĩa.

## 5. Minh chứng

- ireclaim-test.log
- readonly-sha256.log
- usertests-cpus1.log
- usertests-cpus4.log

## 6. Giới hạn và công việc tiếp theo

- Đã kiểm chứng thu hồi 1 inode và 2 data block
  trong kịch bản crash/reboot được thử nghiệm.
- Chưa chứng minh tính đúng đắn với mọi thời điểm crash.
- N16B: phục hồi orphan file vào /lost+found chưa triển khai.
- Tiếp tục N16B trên nhánh riêng sau khi N16A được review.
