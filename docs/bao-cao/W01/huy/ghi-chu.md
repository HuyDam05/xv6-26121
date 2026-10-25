# Baseline W01 - Đàm Quang Huy

- Ngày chạy: 25/10/2026
- Base: xv6-riscv commit 06aad25 (tag upstream-base)
- Máy: HP 245 G8, Ubuntu 24.04
- Toolchain: riscv64-linux-gnu-gcc 13.3.0, QEMU 8.2.2 (chi tiết: versions.txt)

## Kết quả

| Bước | Lệnh | Kết quả |
|---|---|---|
| Build | make qemu | Build thành công, xv6 boot, vào được shell |
| usertests | usertests (3 CPU, mặc định của Makefile) | ALL TESTS PASSED |
| grind | grind | Chạy đến khi dừng thủ công (Ctrl-A, X), không thấy panic |

## Ghi chú
- Lần gõ usertests đầu bị lỗi exec do lẫn ký tự lạ ở đầu dòng; gõ lại thì chạy bình thường.
- Log đầy đủ: qemu-session.log
