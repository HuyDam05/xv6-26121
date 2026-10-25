# Test matrix - Đề tài 26121 (Crash consistency và fsck)

Cập nhật: 25/10/2026. Bản sơ bộ, mọi dòng đang ở trạng thái Planned. Cột "Cách chạy" sẽ cập nhật khi có script.
Quy ước điểm crash: (1) sau write_log, trước write_head; (2) ngay sau write_head; (3) giữa install_trans; (4) sau install_trans, trước khi xóa header; (5) lúc ghi bitmap/inode.

| ID | Nhóm | Kịch bản | Kết quả mong đợi | Cách chạy | Trạng thái |
|---|---|---|---|---|---|
| L01 | Log | Crash tại điểm (1) | Transaction không có hiệu lực, boot lại không redo, fsck clean | crash_loop | Planned |
| L02 | Log | Crash tại điểm (2) | Recovery redo đủ, dữ liệu transaction có mặt, fsck clean | crash_loop | Planned |
| L03 | Log | Crash tại điểm (3) | Recovery redo lại từ đầu, fsck clean | crash_loop | Planned |
| L04 | Log | Crash tại điểm (4) | Redo lặp lại không làm hỏng dữ liệu, fsck clean | crash_loop | Planned |
| L05 | Log | Crash tại điểm (5) | Recovery đưa bitmap/inode về trạng thái nhất quán | crash_loop | Planned |
| L06 | Log/CRC | Commit bình thường, boot lại | Không có cảnh báo CRC | usertests | Planned |
| L07 | Log/CRC | Sửa 1 byte trong log block, giữ n>0 | CRC lệch, bỏ transaction, in cảnh báo, không redo dữ liệu rác | corrupt+boot | Planned |
| L08 | Log/CRC | Header sai magic | Recovery từ chối header, in cảnh báo | corrupt+boot | Planned |
| L09 | Log/CRC | Header có n lớn hơn LOGBLOCKS | Recovery từ chối, không tràn mảng, không panic | corrupt+boot | Planned |
| L10 | Log/CRC | Header ghi dở (cắt nửa) | CRC lệch, bỏ transaction | corrupt+boot | Planned |
| L11 | Log/CRC | crc32("123456789") và chuỗi rỗng | 0xCBF43926 và 0x00000000 | unit (host) | Planned |
| C01 | Crash-injection | Công tắc tắt (CRASH_NONE) | Kernel chạy bình thường, usertests pass | usertests | Planned |
| C02 | Crash-injection | crash_ctl với điểm hoặc số lần không hợp lệ | Trả lỗi, không crash | usertests | Planned |
| C03 | Crash-injection | Crash sau đúng N lần ghi block | Dừng đúng N, cùng N cho cùng kết quả | crash_loop | Planned |
| F01 | fsck | Ảnh gốc | fsck -n báo clean, exit 0 | fsck | Planned |
| F02 | fsck | Superblock sai magic hoặc size | Báo lỗi, exit khác 0 | corrupt+fsck | Planned |
| F03 | fsck | Bitmap: bit 1 thành 0 (block đang dùng bị coi là free) | Phát hiện, -r sửa, chạy lại clean | corrupt+fsck | Planned |
| F04 | fsck | Bitmap: bit 0 thành 1 (leak) | Phát hiện, -r sửa | corrupt+fsck | Planned |
| F05 | fsck | nlink lệch so với số dirent | Phát hiện, -r ghi lại nlink | corrupt+fsck | Planned |
| F06 | fsck | Hai inode cùng trỏ một block | Phát hiện, báo lỗi | corrupt+fsck | Planned |
| F07 | fsck | Inode mồ côi (type khác 0, không dirent) | Phát hiện, -r đưa vào /lost+found | corrupt+fsck | Planned |
| F08 | fsck | Thư mục thiếu . hoặc .. | Phát hiện | corrupt+fsck | Planned |
| F09 | fsck | Dirent trỏ inode rỗng hoặc ngoài phạm vi | Phát hiện, -r xóa dirent | corrupt+fsck | Planned |
| F10 | fsck | addrs hoặc block gián tiếp trỏ ngoài vùng data | Phát hiện | corrupt+fsck | Planned |
| F11 | fsck | Thư mục tạo vòng lặp | Không treo, báo lỗi | corrupt+fsck | Planned |
| F12 | fsck | Chạy fsck -n | Ảnh không bị ghi (checksum trước và sau như nhau) | fsck | Planned |
| F13 | fsck | Thiếu tham số hoặc file không tồn tại | In usage, exit khác 0 | fsck | Planned |
| F14 | fsck | File đã unlink nhưng còn mở lúc crash | Không bị "sửa" nhầm, sau boot fsck clean | crash_loop | Planned |
| M01 | mkfs | Ảnh mới từ mkfs | /lost+found tồn tại, fsck clean | mkfs+fsck | Planned |
| T01 | Stress | crash_loop 10 lần | Không treo, có seed và điểm crash trong CSV | crash_loop | Planned |
| T02 | Stress | 200+ lần crash | Thống kê tỉ lệ fsck clean, so sánh trước và sau CRC | crash_loop | Planned |
| T03 | Stress | Chạy lại với cùng seed | Cùng kết quả | crash_loop | Planned |
| T04 | Stress | CPUS=1 và CPUS=4, mỗi loại 10 lần | Đều pass | crash_loop | Planned |
| T05 | Stress | stressfs nhiều tiến trình ghi đồng thời rồi crash | fsck clean sau reboot | crash_loop | Planned |
| R01 | Rò rỉ | Đếm free block và free inode trước và sau suite | Không đổi | suite | Planned |
| R02 | Hồi quy | usertests của xv6 gốc | ALL TESTS PASSED | usertests | Planned |
| R03 | Hồi quy | grind | Không panic | grind | Planned |
| B01 | Đo đạc | Throughput ghi trước và sau CRC (>=5 lần lặp) | Báo cáo trung bình và độ lệch | bench | Planned |
| B02 | Đo đạc | Thời gian fsck theo kích thước ảnh | Bảng và đồ thị | bench | Planned |
