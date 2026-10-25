# Kế hoạch và backlog dự án 26121

Ngày: 24/10/2026. **Phân công/lịch mốc dự kiến, chờ nhóm thống nhất và bổ sung hạn cuối kỳ.** Nhóm xác nhận tuần khởi động: W1 là 19–25/10/2026. Đề tài: **Crash consistency và fsck (File systems, ★★★)**. Phụ trách đề xuất: **Vũ Khắc Hùng** (vai A: `log.c`, CRC32, crash-injection), **Vũ Tiến Đức** (vai B: `fsck` check & repair), **Đàm Quang Huy** (vai C: quản lý dự án, host script, test matrix & `mkfs /lost+found`). Chi tiết hợp đồng làm việc nằm trong [quy trình nhóm](Quy-trinh-nhom-26121.md).

Thứ tự khởi động thực tế: **mời/accept cả ba thành viên & giảng viên → mỗi người clone, build baseline và lưu log vào `docs/bao-cao/W01/<tên>/` → nhận K01–K04 trên GitHub Projects board → mở PR ghi chú thiết kế tuần 1 & review chéo → phát triển theo mốc**. Xem [sổ tay GitHub](So-tay-GitHub-nhom.md) để thao tác chuẩn nhánh và PR. Việc nào không có trên board hoặc không có commit/PR thì không được tính làm minh chứng trong báo cáo.

## Các mốc có đầu ra kiểm tra được

| Mốc | Điều kiện nghiệm thu | Trạng thái đã đối chiếu 24/10 |
|---|---|---|
| M0 Baseline | Cả ba có môi trường RISC-V + QEMU, chạy `make qemu`, `usertests`, `grind`; lưu log vào `docs/bao-cao/W01/<tên>/`; có board, Figma và PR onboarding | Đã dựng khung repo và build system; đang thu đủ log cá nhân của cả 3 thành viên |
| M1 Log & CRC32 | Mở rộng `struct logheader` (`magic`, `crc`), hàm `crc32` (đa thức `0xEDB88320`) đạt test vector `0xCBF43926`; kiểm tra CRC khi `recover_from_log` | Đã có file khung `crc32.c/.h`, chưa triển khai thuật toán |
| M2 Crash-injection | Biến đếm block writes và công tắc điểm gãy trong `log.c`, `virtio_disk.c`; map `0x100000` (`kvmmap`) và ghi `0x5555` tắt QEMU tức thì | Đã có file khung `crash.c/.h`, chưa gắn hook |
| M3 Proposal & `fsck -n` | Nộp Proposal 2–3 trang (Tuần 3); `mkfs.c` tạo sẵn `/lost+found`; `fsck -n` (chỉ kiểm tra) phát hiện đúng lỗi superblock, inode, bitmap, `nlink`, thư mục | Chưa triển khai |
| M4 `fsck` Repair & Demo | `fsck` sửa tự động bitmap, `nlink`, đưa inode mồ côi vào `/lost+found`, xóa dirent hỏng; chạy script 10–50 lần; Demo 10 phút (Tuần 5) | Chưa triển khai |
| M5 200+ Crash & Bàn giao | Script chạy 200+ vòng lặp crash-and-reboot có lưu seed, ≥ 25 test cases (mức Excellent), corrupted-image tests, stretch (`group commit`/`fsync`) và báo cáo cuối | Chưa đo |

Không dùng ghi chú nháp chưa qua PR/review để đánh dấu hoàn thành mốc code. Mỗi thành viên bắt buộc phải có commit đóng góp thực chất trong mã nguồn C/kernel theo yêu cầu đề bài.

## Backlog để chuyển thành issue nhỏ

Mỗi dòng là một gói việc trên GitHub Projects (có Assignee, Estimate, Status và Definition of Done). Khi bắt đầu code, tách thành PR nhỏ trong 1–2 buổi để dễ review.

| ID | Owner và reviewer | Đầu ra và tiêu chí nghiệm thu | Phụ thuộc |
|---|---|---|---|
| G01 | Huy / cả nhóm | Dựng GitHub Projects (Backlog / In progress / Review / Done); xin phép thầy dùng GitHub Projects thay Jira; cấp quyền đọc repo, board, Figma cho thầy (hạn tuần 2) | Không |
| G02 | Hùng / Đức | Hợp đồng cấu trúc đĩa (`fs.h`): chốt vị trí `magic`, `crc` trong `struct logheader`, quy ước thư mục `/lost+found` và mã thoát của `fsck` | G01 |
| G03 | Huy / Hùng, Đức | Tạo file Figma chung gồm 3 trang: Kiến trúc tổng thể (Huy), Sơ đồ luồng Commit & Recovery (Hùng), Bố cục đĩa `fs.img` (Đức) | G01 |
| E01 | Mỗi người / Huy | Cài toolchain RISC-V & QEMU; chạy `make qemu`, `usertests`, `grind`; lưu log kèm phiên bản vào `docs/bao-cao/W01/<tên>/` | Không |
| E02 | Mỗi người / chéo | Thực hành quy trình Git: tạo nhánh riêng, commit ghi chú thiết kế tuần 1 vào `docs/`, mở PR, review và merge vào `main` | E01 |
| L01 | Hùng / Đức | Đọc `kernel/log.c`; vẽ Figma luồng commit (`begin_op` → `write_log` → `write_head` → `install_trans` → `n=0`, `write_head` → `end_op`) và `recover_from_log`; chỉ rõ commit point | E01 |
| L02 | Hùng / Huy | Liệt kê các điểm đặt crash-injection (sau `write_log`, ngay sau `write_head`, giữa `install_trans`, trước khi xóa header, lúc ghi bitmap/inode) và mở PR tài liệu | L01 |
| L03 | Hùng / Đức | Viết hàm `crc32` (đa thức `0xEDB88320`); kiểm tra trên host với chuỗi `"123456789"` phải ra đúng `0xCBF43926` | G02, E01 |
| L04 | Hùng / Đức | Cập nhật `struct logheader` thêm `magic` và `crc`; tính CRC trên các log block đã ghi xong và mảng block đích; kiểm tra CRC trong `recover_from_log` | L03 |
| C01 | Hùng / Huy | Map địa chỉ `0x100000` (QEMU virt test device) bằng `kvmmap` trong `kvmmake` (`kernel/vm.c`); viết hàm ghi `0x5555` để thoát QEMU ngay lập tức | E01 |
| C02 | Hùng / Huy | Làm khung `crash.c/.h`: biến đếm số lần ghi block, đạt ngưỡng `N` tại điểm gãy đã chọn thì dừng; có công tắc tắt (`CRASH_NONE`) và syscall điều khiển | L02, C01 |
| F01 | Đức / Hùng | Đọc `fs.h`, `fs.c`, `mkfs.c`; vẽ bố cục đĩa (`boot \| super \| log \| inode \| bitmap \| data`) lên Figma; lập bảng 5 loại lỗi, cách phát hiện và cách sửa | E01 |
| F02 | Huy / Đức | Sửa `mkfs/mkfs.c` để tạo sẵn thư mục `/lost+found` ngay khi khởi tạo `fs.img` (đảm bảo Huy có commit đóng góp trực tiếp vào C/filesystem) | G02, F01 |
| F03 | Đức / Hùng | Viết khung `fsck/fsck.c` chạy trên host (chế độ `-n` chỉ kiểm tra, chưa sửa): đọc superblock, kiểm tra magic/size, duyệt inode và đối chiếu bitmap | F01, E01 |
| F04 | Đức / Huy | Tạo `fs.img` hỏng bằng tay hoặc bằng `fsck/corrupt.c` (sửa vài byte bitmap); kiểm chứng `fsck -n` báo clean trên ảnh gốc và báo lỗi đúng trên ảnh hỏng | F03 |
| F05 | Đức / Hùng | Mở rộng `fsck -n` kiểm tra đủ 5 hạng mục: bitmap sai, `nlink` lệch so với dirent thực tế, block trùng giữa 2 inode, inode mồ côi (`type != 0` nhưng không có dirent), thư mục thiếu `.`/`..` | F03 |
| F06 | Đức / cả nhóm | Triển khai chế độ sửa lỗi (`fsck -r`): đặt lại bit bitmap, ghi lại `nlink`, đưa inode mồ côi vào `/lost+found`, xóa dirent hỏng; verify mount lại vào xv6 chạy được | F02, F05 |
| T01 | Huy / Hùng, Đức | Lập test matrix đầy đủ (tối thiểu ≥ 25 test cases cho mức Excellent): gồm điểm crash, loại hỏng đĩa, lệnh chạy và kết quả kỳ vọng | L02, F01 |
| T02 | Huy / Hùng | Thử nghiệm thủ công: chạy QEMU, kill giữa chừng, boot lại cùng `fs.img`; sau đó viết script đầu tiên chạy liên tiếp 10 lần không treo, lưu `seed` và điểm crash | E01, T01 |
| T03 | Huy / Đức | Nâng cấp `scripts/crash_loop.sh` và `user/crashtest.c` chạy tự động 200+ chu kỳ (tạo/xóa/ghi file → crash mid-commit → reboot → `fsck` báo clean) | L04, C02, F06, T02 |
| T04 | Huy / Đức | Bộ kịch bản `scripts/test_corrupt.sh` tiêm đủ các loại lỗi vào `fs.img`, chạy `fsck -r` và xác nhận ảnh đĩa được khôi phục sạch | F04, F06 |
| D01 | Cả ba; Huy tổng hợp | Báo cáo tiến độ hàng tuần kèm ảnh chụp minh chứng (board, PR, log `make qemu`), cập nhật trạng thái task và dẫn link commit | Lặp mỗi tuần |
| D02 | Cả ba; Huy điều phối | Viết Proposal 2–3 trang ở Tuần 3 (cấu trúc dữ liệu, file sẽ sửa, syscall mới, kế hoạch test) — mỗi người viết phần mình phụ trách | L04, C02, F03, T02 |
| D03 | Cả ba | Chuẩn bị slide/kịch bản và thực hiện Demo 10 phút ở Tuần 5 (CRC32 trong log, crash-injection cơ bản, `fsck` phát hiện lỗi chính) | L04, C02, F05 |
| X01 | Hùng, Huy / Đức | Stretch goal: Triển khai syscall `fsync(int fd)` và cơ chế `group commit` trong `kernel/log.c`, đo đạc so sánh số lượt ghi đĩa | Sau khi M4 ổn định |

Đường găng (critical path) của dự án đi qua: **Baseline (M0) → CRC32 & Crash-injection (M1, M2) + `fsck -n` (M3) → `fsck` Repair & Script 10 runs (M4) → 200+ Crash runs & ≥ 25 test cases (M5)**. Phần phát triển `fsck` trên host của Đức và phần `crc32`/`log.c` trong kernel của Hùng hoàn toàn có thể làm song song ngay từ Tuần 2 sau khi chốt `struct logheader` trong `fs.h`.

## Lịch tham khảo tám tuần

| Tuần | Vũ Khắc Hùng (Vai A: Log/CRC/Crash) | Vũ Tiến Đức (Vai B: Host `fsck`) | Đàm Quang Huy (Vai C: PM/Script/Test) | Đầu ra chung |
|---|---|---|---|---|
| **W1** | Đọc `log.c`, vẽ 2 sơ đồ Figma, chốt `logheader` (`magic`, `crc`) và vị trí crash points | Đọc `fs.h/fs.c/mkfs.c`, lập bảng 5 loại lỗi phát hiện/sửa, vẽ bố cục đĩa lên Figma | Dựng GitHub Projects, xin duyệt thay Jira, cấp quyền cho thầy, lập test matrix ≥ 25 cases, tổng hợp báo cáo W1 | **M0 đạt:** Đủ log `usertests`/`grind`, PR ghi chú và ảnh minh chứng của cả 3 người |
| **W2** | Viết `crc32` đạt test vector `0xCBF43926`; map `0x100000` (`kvmmap`) và làm khung biến đếm crash | Viết khung `fsck.c` chế độ `-n` (đọc superblock, duyệt inode, check bitmap); tạo `fs.img` hỏng bitmap để thử | Viết script đầu tiên chạy lặp 10 lần có lưu `seed`; sửa `mkfs.c` tạo thư mục `/lost+found` | Pass test vector CRC32; `fsck -n` phân biệt đúng ảnh gốc/ảnh hỏng; script chạy 10 lần không treo |
| **W3** | Tích hợp CRC32 vào `write_head` & `recover_from_log`; viết phần Proposal về Log & Crash | Mở rộng `fsck -n` kiểm tra `nlink`, block trùng, inode mồ côi, thư mục `.`/`..`; viết phần Proposal về `fsck` | Kết nối syscall `crash_ctl` vào script; viết phần Proposal về Test Plan & tổng hợp nộp | **M1, M2 & M3:** Nộp **Proposal 2–3 trang** đúng hạn Tuần 3 |
| **W4** | Gắn đầy đủ các điểm crash-injection vào `log.c` và `virtio_disk.c`; kiểm thử mid-commit | Triển khai chế độ sửa lỗi (`fsck -r`) cho bitmap và `nlink`; hoàn thiện `corrupt.c` | Viết `user/crashtest.c` tạo tải đa dạng (tạo, xóa, link file, thư mục) | Hệ thống tự khôi phục đúng khi crash trước và sau commit point |
| **W5** | Tinh chỉnh log recovery & hỗ trợ kiểm thử tích hợp; chuẩn bị phần demo CRC32/Crash | Hoàn thiện `fsck` đưa inode mồ côi vào `/lost+found` và sửa dirent hỏng; chuẩn bị demo `fsck` | Chạy kiểm thử toàn bộ test matrix, tổng hợp kết quả và điều phối kịch bản trình bày | **M4:** Hoàn thành **Demo 10 phút (Tuần 5)** |
| **W6** | Xử lý các góc khuất (edge cases) khi crash giữa `install_trans` hoặc ghi bitmap/inode | Kiểm thử chéo `fsck -r` với mọi chế độ phá hỏng của `corrupt.c` | Mở rộng `crash_loop.sh` chạy tự động **200+ lần crash** với seed ngẫu nhiên | Vượt qua 200+ lần crash liên tiếp, `fsck` báo clean sau mỗi lần reboot |
| **W7** | Triển khai Stretch Goal: `fsync` và `group commit` trong `kernel/log.c` | Kiểm tra tính tương thích của `fsck` với `group commit`, tối ưu hóa báo cáo lỗi | Thu thập toàn bộ log 200+ runs, thống kê tỷ lệ pass/repair và kiểm tra lại ≥ 25 test cases | Đủ dữ liệu thực nghiệm và hoàn thành mục tiêu mở rộng |
| **W8** | Viết phần báo cáo cuối về kiến trúc WAL, CRC32, Crash-injection và Stretch | Viết phần báo cáo cuối về thuật toán `fsck` và kết quả sửa lỗi ảnh đĩa hỏng | Tổng hợp báo cáo cuối kỳ, hướng dẫn reproduce (`README.md`) và rà soát checklist bàn giao | **M5:** Hoàn tất toàn bộ deliverables và nộp bài |

## Bốn đầu ra cho tuần khởi động W1

Các dòng dưới là việc **cần hoàn tất trong tuần 19–25/10** để đưa vào Báo cáo Tuần 1:

| ID | Owner | Đầu ra nhỏ trong W1 | Liên hệ backlog |
|---|---|---|---|
| K01 | Vũ Khắc Hùng | Log baseline cá nhân (`usertests`, `grind`); 2 sơ đồ Figma (commit & `recover_from_log`); PR tài liệu ghi rõ commit point, chỗ thêm `magic`/`crc` trong `logheader` và 5 vị trí đặt crash-injection | E01, E02, L01, L02 |
| K02 | Vũ Tiến Đức | Log baseline cá nhân; sơ đồ bố cục đĩa trên Figma; PR tài liệu bảng 5 loại lỗi (Bitmap sai, `nlink` sai, Block trùng, Inode mồ côi, Thư mục hỏng), chốt thiết kế `fsck` trên host và ghi chú `/lost+found` | E01, E02, F01 |
| K03 | Đàm Quang Huy | Log baseline cá nhân; dựng GitHub Projects board (đủ cột và item cho 3 người); tạo Figma chung; lập test matrix sơ bộ (hướng tới ≥ 25 cases); thử kill QEMU thủ công và mở PR | G01, G03, E01, E02, T01 |
| K04 | Đàm Quang Huy điều phối, cả ba | Xin phép thầy dùng GitHub Projects, cấp quyền đọc repo/board/Figma cho thầy; chụp ảnh minh chứng (board, PR, `make qemu`) và hoàn thiện Báo cáo Tuần 1 | G01, D01 |

## Bốn task nhỏ dự kiến cho kỳ tiếp theo W2

Bốn task này được đưa vào mục **TODO Tuần 2** của Báo cáo Tuần 1, có tiêu chí hoàn thành (Definition of Done) rõ ràng:

| ID báo cáo | Owner | Đầu ra được kiểm tra ở cuối W2 | Liên hệ backlog |
|---|---|---|---|
| N01 | Vũ Khắc Hùng | Hàm `crc32` (đa thức `0xEDB88320`) chạy trên host với `"123456789"` ra đúng `0xCBF43926`; PR cập nhật `logheader` mới; khung biến đếm crash-injection và map địa chỉ `0x100000` (`kvmmap`) để ghi `0x5555` thoát QEMU | L03, L04, C01, C02 |
| N02 | Vũ Tiến Đức | Khung `fsck/fsck.c` chạy trên host với chế độ `-n` (đọc superblock, kiểm tra magic/kích thước, duyệt inode, kiểm tra bitmap); thử nghiệm thực tế báo clean trên `fs.img` gốc và báo lỗi trên `fs.img` đã bị sửa bitmap | F03, F04 |
| N03 | Đàm Quang Huy | Script tự động đầu tiên (`scripts/crash_loop.sh`) copy `fs.img`, chạy QEMU với workload tạo/xóa/ghi file, kill ngẫu nhiên, boot lại, chạy `fsck` liên tiếp 10 lần không treo, có ghi log `seed` và điểm crash | T02 |
| N04 | Đàm Quang Huy (cùng Hùng, Đức) | PR sửa `mkfs/mkfs.c` tạo sẵn thư mục `/lost+found` trên đĩa (commit kernel/fs của Huy); review chéo toàn bộ PR tuần 2 và hoàn tất cấp quyền truy cập cho giảng viên | F02, G01, D01 |

## Bước nhóm thực hiện ngay

1. **Mỗi thành viên (Hùng, Đức, Huy):** Chạy `make qemu`, chạy `usertests` và `grind`, lưu toàn bộ terminal output kèm lệnh `riscv64-linux-gnu-gcc --version` và `qemu-system-riscv64 --version` vào `docs/bao-cao/W01/<tên>/`.
2. **Mở Pull Request cá nhân:** Đẩy ghi chú thiết kế Tuần 1 của phần mình vào `docs/`, tạo PR trên GitHub và gắn thẻ thành viên khác vào review trước khi merge.
3. **Đàm Quang Huy:** Kiểm tra lại toàn bộ item trên GitHub Projects board (đảm bảo mỗi người đều có item gắn Assignee, Estimate, Status), chụp màn hình minh chứng và xuất bản Báo cáo Tuần 1.
