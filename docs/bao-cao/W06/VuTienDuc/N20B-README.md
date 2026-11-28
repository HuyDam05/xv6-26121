# W06 - N20B: Multi-orphan Recovery

## 1. Thông tin

- Sinh viên: Vũ Tiến Đức
- Nhiệm vụ: N20B
- Branch: feat/duc-w06-multi-orphan
- Base commit: f562803
- File triển khai: fsck/fsck.c
- Hàm chỉnh sửa: recover_orphans()

## 2. Mục tiêu

Mở rộng chức năng phục hồi orphan inode từ một file
sang tối đa ba regular file trong một lần sửa chữa.

Bảo đảm kiểm tra toàn bộ danh sách candidate trước
khi ghi dữ liệu xuống ảnh đĩa.

## 3. Nội dung triển khai

- Thu thập danh sách orphan inode.
- Giới hạn tối đa ba orphan trong một lần phục hồi.
- Chỉ phục hồi regular file có nlink=1.
- Không phục hồi inode nlink=0 đang chờ kernel thu hồi.
- Kiểm tra direct block và indirect block.
- Kiểm tra bitmap của các block thuộc candidate.
- Kiểm tra tham chiếu từ các thư mục đã cấp phát.
- Kiểm tra trùng entry lost+found.
- Kiểm tra tên file phục hồi và dung lượng thư mục.
- Lập kế hoạch phục hồi trong bộ nhớ trước khi ghi.
- Không cấp phát data block mới.
- Hỗ trợ tiếp tục một trạng thái interrupted repair.
- Sử dụng fsck-safe-repair.py để sửa trên ảnh tạm
  và xác minh trước khi thay thế ảnh nguồn.

## 4. Kết quả kiểm thử

### 4.1. Positive tests

| Test | Kết quả |
|---|---|
| Clean filesystem | PASS |
| Phục hồi hai orphan | PASS |
| Phục hồi ba orphan | PASS |
| Bảo toàn nội dung file | Xem data-integrity.log |
| Idempotence | PASS |
| Interrupted repair | PASS |

### 4.2. Safety tests

| Test | Hành vi yêu cầu |
|---|---|
| NEG01 | Từ chối khi không đủ append slot |
| NEG02 | Từ chối khi trùng tên phục hồi |
| NEG03 | Giữ nguyên orphan nlink=0 |
| NEG04 | Từ chối orphan directory |
| NEG05 | Từ chối duplicate block |
| NEG06 | Từ chối bốn orphan |
| NEG07 | Tiếp tục trạng thái interrupted repair |
| NEG08 | Từ chối block không hợp lệ |

Chi tiết kết quả được lưu trong các file log.
Các trường hợp bị từ chối được kiểm tra SHA-256
để xác minh ảnh nguồn không thay đổi.

### 4.3. Regression

- Host tests: 19/19 PASS.
- Log recovery tests: 6/6 PASS.
- Tổng regression: 25/25 PASS.
- Usertests CPUS=1: ALL TESTS PASSED.
- Usertests CPUS=4: ALL TESTS PASSED.
- Local format check: PASS.

## 5. Giới hạn

- Chỉ hỗ trợ tối đa ba regular file có nlink=1.
- Không phục hồi orphan directory.
- Không cấp phát thêm block cho lost+found.
- Chỉ append trong block đầu tiên đã được cấp phát.
- Không xử lý mọi dạng hỏng filesystem.
- Hai thao tác ghi trực tiếp của fsck -y không
  bảo đảm tính nguyên tử trong mọi tình huống.
- Interrupted repair được kiểm thử bằng trạng thái
  ảnh đĩa mô phỏng, không phải mất điện vật lý.
- Safe repair yêu cầu ảnh đĩa offline và không có
  tiến trình khác ghi đồng thời.

## 6. Minh chứng

- N20B-orphan-repair.log
- N20B-negative.log
- N20B-preflight.log
- N20B-data-integrity.log
- N20B-interrupted-idempotence.log
- N20B-regression.log
- N20B-usertests-cpus1.log
- N20B-usertests-cpus4.log

## 7. Kết luận

Đã triển khai chức năng phục hồi đồng thời hai
và ba orphan file trong phạm vi hỗ trợ.

Các bài regression và usertests không phát hiện
lỗi phát sinh trong phạm vi kiểm thử.

Ưu tiên sử dụng safe repair để bảo vệ ảnh nguồn
khi quá trình sửa chữa không thành công.

