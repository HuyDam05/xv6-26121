#ifndef XV6_CRASH_H
#define XV6_CRASH_H

#define CRASH_NONE              0
#define CRASH_COMMIT_BEFORE_HDR 1
#define CRASH_COMMIT_AFTER_HDR  2
#define CRASH_INSTALL_MID       3
#define CRASH_DISK_WRITE        4
#define CRASH_INSTALL_DONE      5
// diem crash hop le lon nhat; crash_ctl chi nhan 0..CRASH_MAXPOINT
#define CRASH_MAXPOINT CRASH_INSTALL_DONE

void crash_init(void);
void crash_set(int point, int count);
void crash_check(int point);

#endif
