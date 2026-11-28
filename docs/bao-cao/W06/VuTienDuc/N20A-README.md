# W06 - N20A: Ireclaim Stress Test

## 1. Thông tin

- Người thực hiện: Vũ Tiến Đức
- MSSV: 20233340
- Base commit: 47bf006
- Branch: feat/duc-w06-ireclaim-stress

## 2. Nội dung triển khai

- Phân tích fsinit(), ireclaim(), iput(), itrunc()
  và ifree() trong kernel/fs.c.
- Mở rộng user/ireclaimtest.c từ một file lên 8 file.
- Mỗi file sử dụng 2 block dữ liệu.
- Unlink 8 file nhưng giữ nguyên file descriptor.
- Tự động hóa crash/reboot bằng ireclaim_stress.py.
- Sử dụng ảnh đĩa riêng cho mỗi lượt kiểm thử.
- Kiểm tra SHA-256 trước và sau fsck -n.

## 3. Bất biến cần kiểm chứng

- Inode có nlink=0 nhưng còn fd mở chưa được thu hồi.
- Sau crash/reboot, ireclaim thu hồi inode và block.
- Số block/inode trống trở về đúng baseline.
- Các file đã unlink không còn reachable.
- fsck -n không thay đổi ảnh đĩa.

## 4. Kết quả

| Test | Result |
|---|---|
| Stress CPUS=1 | 10/10 PASS |
| Stress CPUS=4 | 10/10 PASS |
| Total stress | 20/20 PASS |
| Reclaimed inodes | 160 |
| Reclaimed blocks | 320 |
| Block leak | 0 |
| Inode leak | 0 |
| Host regression | 19/19 PASS |
| Log regression | 6/6 PASS |
| Usertests CPUS=1 | ALL TESTS PASSED |
| Usertests CPUS=4 | ALL TESTS PASSED |

## 5. Phân tích

Trong phạm vi kiểm thử, kernel thu hồi đầy đủ
8 inode và 16 block sau mỗi lượt crash/reboot.

Không phát hiện lỗi kernel cần sửa.
Không thay đổi mã nguồn kernel.

Trong giai đoạn phát triển script, phát hiện lỗi regex
đọc baseline chưa đầy đủ. Đã sửa bằng cách yêu cầu
marker kết thúc bằng newline trước khi lấy tham số.

## 6. Minh chứng

- ireclaim-stress-cpus1.log
- ireclaim-stress-cpus4.log
- cpus1-run01.log đến cpus1-run10.log
- cpus4-run01.log đến cpus4-run10.log
- regression.log
- usertests-cpus1.log
- usertests-cpus4.log

## 7. Giới hạn

- Stress test sử dụng 8 orphan file mỗi lượt.
- Crash được mô phỏng bằng SIGKILL tiến trình QEMU.
- Chưa kiểm thử mọi thời điểm crash trong transaction.
- Không kết luận kernel an toàn trong mọi tình huống.
- Chưa đo rò rỉ free-page RAM.
