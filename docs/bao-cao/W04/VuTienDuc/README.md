# Báo cáo cá nhân W04 – Vũ Tiến Đức

**Dự án:** Crash Consistency and fsck trên xv6-riscv
**Nhóm:** 26121
**Thời gian:** 09/11–15/11/2026
**Nhiệm vụ:** N11 – Phát triển chức năng sửa lỗi `fsck -y`

## 1. Công việc thực hiện

- Mở rộng `fsck` hỗ trợ hai chế độ `-n` và `-y`.
- Giữ nguyên chế độ `-n` chỉ đọc.
- Triển khai sửa bitmap bị đánh dấu sai.
- Triển khai sửa số liên kết inode (`nlink`).
- Từ chối sửa khi phát hiện block trùng hoặc orphan inode.
- Kiểm thử chạy lại `fsck -n` sau khi sửa.
- Kiểm tra tính idempotent của chế độ `-y`.

## 2. Kết quả kiểm thử

| Trường hợp | Kết quả |
|---|---|
| bitmap-clear | PASS – sửa 1 bit |
| bitmap-set | PASS – sửa 1 bit |
| nlink-bump | PASS – sửa 1 nlink |
| dup-block | PASS – từ chối sửa |
| dirent-drop | PASS – từ chối sửa |
| fsck -n SHA-256 | PASS – không thay đổi ảnh |
| fsck -y lần hai | PASS – không sửa thêm |
| Boot QEMU sau sửa | PASS – shell hoạt động, ls thành công |

## 3. Cơ chế an toàn

- Chế độ `-n` mở ảnh bằng `rb`, không thực hiện thao tác ghi.
- Chế độ `-y` mở ảnh bằng `r+b` và từ chối tên ảnh `fs.img`.
- Trước khi sửa, kiểm tra cấu trúc inode, thư mục và orphan.
- Không tự sửa block trùng hoặc các trường hợp không đủ căn cứ.
- Các kiểm thử đều sử dụng bản sao ảnh đĩa.

## 4. Minh chứng

- `repair-tests.log`: phát hiện, sửa lỗi và kiểm thử idempotence.
- `readonly-sha256.log`: xác minh chế độ `-n` chỉ đọc.
- `qemu-boot.log`: xác minh xv6 khởi động từ ảnh sau sửa.

## 5. Công việc chưa hoàn thành

- Chưa tự động đưa orphan inode vào `/lost+found`.
- Chưa kiểm thử đầy đủ mọi trường hợp ảnh đĩa hỏng.
- Chuyển phục hồi orphan nâng cao sang W05.

## 6. Kết luận

Đã triển khai chức năng `fsck -y` sửa bitmap và nlink trên
ảnh đĩa xv6. Các trường hợp kiểm thử trọng tâm đạt yêu cầu,
ảnh sau sửa được `fsck -n` xác nhận sạch và có thể boot QEMU.
