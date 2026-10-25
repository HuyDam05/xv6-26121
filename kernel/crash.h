#ifndef XV6_CRASH_H
#define XV6_CRASH_H

#define CRASH_NONE              0
#define CRASH_COMMIT_BEFORE_HDR 1
#define CRASH_COMMIT_AFTER_HDR  2
#define CRASH_INSTALL_MID       3
#define CRASH_DISK_WRITE        4

void crash_init(void);
void crash_set(int point, int count);
void crash_check(int point);

#endif
