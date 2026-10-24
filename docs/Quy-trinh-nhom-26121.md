# Quy trình làm việc nhóm - 26121

## 1. Quy ước Git
- Nhánh chính: `main` (chỉ chứa code đã test qua `make qemu`).
- Nhánh tính năng: `feat/crc32`, `feat/fsck`, `feat/crash-injection`.
- Commit format: `feat: ...`, `fix: ...`, `docs: ...`, `test: ...`.

## 2. Quy trình Review
- Tạo Pull Request trước khi merge vào `main`.
- Đảm bảo `make kernel/kernel fs.img fsck/fsck` không phát sinh lỗi hoặc cảnh báo (`-Werror`).
