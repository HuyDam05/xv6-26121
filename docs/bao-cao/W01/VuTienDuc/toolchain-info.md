# Thông tin Toolchain & Môi trường - Vũ Tiến Đức

## 1. Các lệnh đã chạy
- Kiểm tra trình biên dịch: `riscv64-linux-gnu-gcc --version`
- Kiểm tra QEMU: `qemu-system-riscv64 --version`
- Lệnh chạy kiểm thử: `make qemu`, sau đó trong shell xv6 chạy `usertests -q` và `grind`.

## 2. Phiên bản Toolchain thực tế
```text
riscv64-linux-gnu-gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3.2)
Linux vutienduc-20233340-thinkbook-14-g8-agp 7.2.6 #1 SMP PREEMPT_DYNAMIC Mon Sep 28 21:13:54 +07 2026 x86_64 GNU/Linux
```
