# DESIGN PROPOSAL – PROJECT 26121

**Crash Consistency and fsck on xv6-riscv**  
**Học kỳ:** 20261 | **Mốc:** Tuần 3, 02–08/11/2026  
**Nhóm:** Đàm Quang Huy (20233439) – PM / Kernel diagnostics / Testing; Vũ Tiến Đức (20233340) – fsck; Vũ Khắc Hùng (20233428) – WAL / CRC / Crash.

## 1. Mục tiêu và kiến trúc

Xv6-riscv sử dụng write-ahead log (WAL) để ghi các thao tác filesystem theo transaction. Đề tài 26121 mở rộng độ tin cậy khi máy dừng đột ngột bằng ba thành phần: **(i)** CRC32 và kiểm tra log trước recovery; **(ii)** công cụ fsck trên Ubuntu để phát hiện, sau đó sửa các metadata sai; **(iii)** cơ chế crash-injection và bộ test tự động, có thể tái lập bằng seed. **Bổ sung từ W4:** module `fsusage` chạy trong kernel, do Huy phụ trách, cung cấp số liệu sử dụng block/inode cho kiểm thử rò rỉ và đối chiếu với fsck. Đây là công cụ hỗ trợ đánh giá, không thay thế các yêu cầu cốt lõi của đề tài.

**Luồng kiểm thử:** tạo bản sao `fs.img` → QEMU chạy `crashtest` → crash → boot lại **cùng ảnh** → `fsck -n` trên host → kiểm tra PASS/FAIL và ghi CSV; `fsusage` cung cấp thêm số liệu khi xv6 đang hoạt động.

## 2. Data structures and design

**Design Diagrams (Figma/FigJam):**

Các sơ đồ thiết kế của nhóm được tổng hợp trên Figma,
bao gồm kiến trúc hệ thống, quy trình commit/recovery,
bố cục filesystem và luồng kiểm thử.

[Project 26121 – Design Diagrams (Figma)](https://www.figma.com/board/tRPEkizJzKqFzMnn73o1wy/26121---Crash-consistency-and-fsck?node-id=0-1&t=mHsQgE0G3dilgqok-1)

### 2.1. WAL, CRC và crash-injection (Hùng)

`struct logheader`: `n` (4 B), `block[30]` (120 B), `magic` (4 B), `crc` (4 B), tổng **132 B < BSIZE = 1024 B**. CRC32 (đa thức `0xEDB88320`) bao phủ `n`, các block đích và nội dung log block. Trình tự commit: `write_log()` → `write_head()` (**commit point**) → `install_trans()` → xóa header. Recovery chỉ replay transaction có `n` hợp lệ, `magic` đúng và CRC khớp. Năm điểm crash: **1** trước header, **2** sau header, **3** giữa install, **4** khi `bwrite`, **5** sau install nhưng trước khi xóa header.

### 2.2. fsck trên host (Đức)

`superblock`, `dinode`, `dirent` mô tả bố cục đĩa, inode và mục thư mục. `used_blocks[]` phát hiện block trùng/bitmap sai; `links[]` đối chiếu `nlink`; `parent[]` kiểm tra quan hệ thư mục; `reachable[]` và `queue[]` phục vụ BFS phát hiện orphan. `fsck -n` là chế độ chỉ đọc. `fsck -y` **dự kiến W4–W5** sửa bitmap/nlink khi có căn cứ, phục hồi inode phù hợp vào `/lost+found`; không tự ý sửa block trùng hoặc quan hệ thư mục mơ hồ.

### 2.3. fsusage trong kernel (Huy, đề xuất mới W4–W5)

```c
struct fsusage {
  uint total_blocks, free_blocks;  // chỉ data blocks
  uint total_inodes, free_inodes;  // inode hợp lệ: 1..ninodes-1
};
```

Kernel đọc các bit bitmap ứng với data block từ `data_start = sb.bmapstart + ceil(sb.size/BPB)` đến `sb.size - 1`, đồng thời duyệt inode trên đĩa (`type == 0` là free), qua `bread()`/`brelse()` từng block; **không ghi metadata**. Chỉ số là ảnh chụp gần đúng khi có ghi đồng thời; các phép so sánh chính xác trước/sau workload được thực hiện lúc filesystem nghỉ. Free pages thuộc bộ cấp phát RAM, **không** nằm trong `fsusage`; phép kiểm tra free-page leak cần nguồn đo riêng.

<!-- PAGE BREAK -->

## 3. Files touched and ownership

| Thành phần / Owner | File sẽ sửa hoặc đã sửa | Mục đích |
|---|---|---|
| Hùng – WAL/CRC | `kernel/log.c`, `kernel/crc32.c/.h` | CRC32, log header, recovery |
| Hùng – crash | `kernel/crash.c/.h`, `kernel/bio.c`, `user/crashtest.c` | 5 crash hooks, workload |
| Đức – fsck | `fsck/fsck.c`, `kernel/fs.h` | Kiểm tra inode, bitmap, nlink, thư mục; kế hoạch repair |
| **Đức – kernel W5** | **kernel/fs.c (ireclaim), kernel/defs.h (nếu cần)** | **Kiểm tra và củng cố xử lý orphan sau crash** |
| Huy – host tools | `mkfs/mkfs.c`, `fsck/corrupt.c`, `scripts/run_tests.sh`, `scripts/crash_loop.sh`, `Makefile` | `/lost+found`, ảnh lỗi, test và CSV |
| **Huy – kernel mới** | **kernel/fs.c, kernel/fs.h, kernel/sysfile.c, kernel/defs.h** | **Quét thống kê filesystem và trả kết quả** |
| **Huy – syscall mới** | **kernel/syscall.h/.c, user/user.h, user/usys.pl, user/fsusagetest.c** | **Đăng ký API, wrapper và test** |

Phần `fsusage` của Huy được tách thành PR W4. Để mỗi người đều có đóng góp kernel, **Đức nhận thêm PR W5 về xử lý orphan khi boot (`ireclaim`)**: phân tích các trường hợp `nlink = 0`, bổ sung kiểm tra an toàn khi cần, kèm crash test và chứng minh không hồi quy. Hai người sửa các vùng chức năng khác nhau trong `kernel/fs.c`, merge tuần tự và review chéo. Hùng tiếp tục sở hữu log/CRC/recovery.

## 4. New system calls

**Đã triển khai:** `int crash_ctl(int point, int count)` – syscall **23**. `point = 0` tắt crash, 1–5 chọn vị trí; `count` chỉ số lần kích hoạt tại điểm đó. Trả `0` khi hợp lệ và `-1` nếu tham số sai. `crashtest` sử dụng syscall để gây crash trong workload, không cần biên dịch lại kernel giữa các lượt.

**Đề xuất triển khai W4–W5:** `int fsusage(struct fsusage *info)` – số syscall **25 dự kiến** (phải kiểm tra lại bảng syscall trước khi merge). Kernel kiểm tra con trỏ user, duyệt metadata chỉ đọc, dùng `copyout()` trả thống kê; con trỏ không hợp lệ trả `-1`. `total_blocks/free_blocks` chỉ tính data block; `total_inodes/free_inodes` chỉ tính inode 1 đến `ninodes-1`. Không giữ khóa buffer qua các lần đọc dài và không hứa hẹn snapshot nguyên tử khi filesystem đang được ghi.

**Các bất biến:** `0 ≤ free_blocks ≤ total_blocks`; `0 ≤ free_inodes ≤ total_inodes`; API không thay đổi `fs.img`; các thao tác đọc không gây deadlock với đường ghi log. Test trước/sau chỉ đo khi không còn tiến trình ghi file.

<!-- PAGE BREAK -->

## 5. Test plan and acceptance criteria

| Nhóm kiểm thử | Cách thực hiện / tiêu chí PASS |
|---|---|
| CRC / WAL | Test vector CRC32 `123456789 → 0xCBF43926`; sai magic/CRC hoặc header bất thường bị từ chối, không replay dữ liệu hỏng. |
| Crash recovery | Các điểm 1–5; crash trước commit không áp transaction; sau commit replay được; reboot cùng ảnh, không panic, `fsck -n` clean. |
| fsck / corruption | `make test` chạy tự động trên ảnh sạch, bitmap-clear/set, nlink-bump, dup-block, dirent-drop; kiểm tra exit code, thông báo và checksum bất biến của `-n`. |
| `fsusage`: hợp lệ | Đọc thống kê lúc khởi động; giá trị trong giới hạn; so sánh với bitmap/inode trên ảnh đĩa ổn định. |
| `fsusage`: vòng đời file | Đo trước/sau create–write–close–unlink; free inode/block giảm rồi trở lại mức nền khi hệ thống nghỉ; kiểm tra cả giới hạn hết chỗ. |
| `fsusage`: lỗi và đa CPU | Con trỏ NULL/sai trả `-1`, không panic; chạy `CPUS=1` và `CPUS=4`, mỗi cấu hình ≥10 lần; không deadlock/hồi quy. |
| Leak / regression | So sánh free inode và free page trước/sau suite (free page cần đo bổ sung); test orphan `nlink=0` sau reboot; chạy stock `usertests` và `grind`. |
| Stress / đánh giá | ≥200 crash có seed, ghi CSV từng lượt; ≥25 test có PASS/FAIL ở mức mục tiêu; benchmark lặp ≥5 lần/điểm dữ liệu, báo trung bình và độ phân tán. |

**Kết quả đã xác nhận ở W3:** `make test` có **6/6 PASS**; crash-loop dùng `crash_ctl` có **10/10 lượt crash → reboot → fsck clean**, hiện ở `CPUS=1`. Đây là minh chứng bước đầu, **không** thay thế kiểm thử 1/4 CPU, 200+ lượt, regression và benchmark cuối kỳ.

## 6. Milestones, risks and responsibilities

**W4:** Huy triển khai `fsusage` trong kernel, Đức bắt đầu `fsck -y`, Hùng mở rộng ca log hỏng. **W5:** Huy kiểm thử `fsusage` 1/4 CPU; Đức nhận PR kernel riêng củng cố `ireclaim`/orphan recovery sau crash; cả nhóm tích hợp demo giữa kỳ. **W6–W7:** 200+ crash, kiểm tra leak, stress và benchmark. **W8:** code freeze; **W9:** demo và báo cáo cuối kỳ. Mỗi phần code có PR được thành viên khác review và log kiểm thử tái lập.

**Rủi ro chính:** số liệu `fsusage` không nguyên tử khi ghi đồng thời; sửa filesystem thiếu căn cứ; hoặc crash test chưa bao phủ đầy đủ trường hợp biên. Giảm thiểu bằng đo ở trạng thái nghỉ, chỉ sửa lỗi fsck khi an toàn, dùng ảnh đĩa bản sao, seed cố định, kiểm thử hồi quy và review chéo.

**Phân công kernel:** Hùng – WAL/CRC/crash; Đức – **orphan reclaim (`ireclaim`) trong kernel**, song song với fsck/repair trên host; Huy – **kernel filesystem diagnostics (`fsusage`) và syscall**. Huy đồng thời điều phối test matrix, GitHub Projects và báo cáo. Phần mở rộng của Đức cần được cụ thể hóa thành PR có mã kernel và test riêng ở W5; chỉ review không đủ.

**Nguồn nội bộ:** `docs/proposal-hung.md`, `docs/bao-cao/W03/VuTienDuc/fsck-proposal.md`, `docs/test-matrix.md`, các PR #22, #29–#31. Sơ đồ kiến trúc, commit/recovery và bố cục đĩa cần đính kèm hoặc liên kết Figma của nhóm khi nộp.
