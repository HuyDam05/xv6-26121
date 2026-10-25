# Baseline W01 - Vũ Khắc Hùng

- Ngày chạy: 25/10/2026
- Base: xv6-riscv commit 06aad25 (tag upstream-base)
- Máy: Dell G15 5511, Ubuntu 24.04.5 LTS
- Toolchain: riscv64-linux-gnu-gcc 13.3.0, QEMU 8.2.2 (chi tiết: versions.txt)

## Kết quả

| Bước | Lệnh | Kết quả |
|---|---|---|
| Build | make qemu | Build thành công, xv6 boot, vào được shell |
| usertests | make qemu CPUS=1, rồi usertests -q | ALL TESTS PASSED (log: qemu-usertests-cpu1.log) |
| usertests | make qemu CPUS=4, rồi usertests -q | ALL TESTS PASSED (log: qemu-usertests-cpu4.log) |
| grind | make qemu CPUS=4, rồi grind | Chạy khoảng 2 phút, không thấy panic, thoát thủ công (Ctrl-A, X); ghi chú: grind.txt |

## Ghi chú
- Repo của nhóm được reset về xv6-riscv gốc sau lần chạy đầu, nên baseline được chạy lại trên commit 06aad25 (hash lưu trong commit-hash.txt).
- Lần chạy đầu bị lỗi do còn nhiều phiên script chạy chồng nhau và một tiến trình QEMU cũ giữ khóa fs.img (báo lỗi Failed to get write lock). Đã dọn tiến trình thừa và chạy lại từng bước.
