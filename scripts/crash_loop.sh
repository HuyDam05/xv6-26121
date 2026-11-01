#!/usr/bin/env bash
# crash_loop.sh - chay xv6, crash o diem ngau nhien (theo seed), boot lai cung fs.img, chay fsck, ghi CSV.
# Cach dung (chay tu thu muc goc repo):
#   bash scripts/crash_loop.sh [RUNS]
# Bien moi truong tuy chon:
#   RUNS=10  SEED_BASE=1  CPUS=1  POINT=4  NMAX=3000  WORKLOAD="usertests -q"  TIMEOUT=120
# Tai lap mot lan chay: RUNS=1 SEED_BASE=<seed> bash scripts/crash_loop.sh
set -u

RUNS=${1:-${RUNS:-10}}
SEED_BASE=${SEED_BASE:-1}
CPUS=${CPUS:-1}
POINT=${POINT:-4}            # 4 = CRASH_DISK_WRITE (xem kernel/crash.h)
NMAX=${NMAX:-3000}           # crash o lan ghi block thu N, N chon ngau nhien trong [1, NMAX]
WORKLOAD=${WORKLOAD:-"usertests -q"}
TIMEOUT=${TIMEOUT:-120}
REBOOT_SECS=${REBOOT_SECS:-8}

WORK=results/work
CSV=results/crash_runs.csv
mkdir -p "$WORK/logs" results

for tool in qemu-system-riscv64 make; do
  command -v "$tool" >/dev/null || { echo "thieu $tool"; exit 1; }
done

qemu_run() { # $1=kernel $2=image $3=giay toi da $4=lenh nhap vao xv6 $5=file log
  timeout "$3" qemu-system-riscv64 -machine virt -bios none -kernel "$1" -m 128M \
    -smp "$CPUS" -nographic -global virtio-mmio.force-legacy=false \
    -drive file="$2",if=none,format=raw,id=x0 \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
    < <(exec 2>/dev/null; sleep 2; printf '%s\n' "$4"; sleep "$3") > "$5" 2>&1
}

echo "[crash_loop] build ban kernel sach (khong crash) va fs.img goc"
touch kernel/crash.c
make -s kernel/kernel fs.img fsck/fsck >/dev/null 2>&1 || { echo "build loi"; exit 1; }
cp kernel/kernel "$WORK/kernel-clean"
cp fs.img "$WORK/fs.base.img"

[ -f "$CSV" ] || echo "run,seed,cpus,point,n,crashed,reboot_ok,recovering_lines,crc_msgs,panic,fsck_exit,fsck_last_line" > "$CSV"

clean=0; bad=0; nocrash=0
for ((i = 0; i < RUNS; i++)); do
  seed=$((SEED_BASE + i))
  RANDOM=$seed
  n=$((RANDOM % NMAX + 1))
  tag="s${seed}_p${POINT}_n${n}_c${CPUS}"

  # 1) build kernel co crash-injection tai lan ghi thu n
  touch kernel/crash.c
  make -s kernel/kernel DETFLAGS="-ffile-prefix-map=$PWD=. -DCRASH_DEFAULT_POINT=$POINT -DCRASH_DEFAULT_COUNT=$n" >/dev/null 2>&1 \
    || { echo "build kernel crash loi"; exit 1; }
  cp kernel/kernel "$WORK/kernel-crash"

  # 2) chay workload tren anh sach cho den khi crash
  cp "$WORK/fs.base.img" "$WORK/work.img"
  qemu_run "$WORK/kernel-crash" "$WORK/work.img" "$TIMEOUT" "$WORKLOAD" "$WORK/logs/$tag.crash.log"
  crashed=0; grep -q "CRASH: point=" "$WORK/logs/$tag.crash.log" && crashed=1

  # 3) boot lai CUNG anh bang kernel sach (khong crash) de recovery chay
  qemu_run "$WORK/kernel-clean" "$WORK/work.img" "$REBOOT_SECS" "ls" "$WORK/logs/$tag.reboot.log"
  rlog="$WORK/logs/$tag.reboot.log"
  reboot_ok=0; grep -q "init: starting sh" "$rlog" && grep -q "README" "$rlog" && reboot_ok=1
  rec=$(grep -c "recovering tail" "$rlog")
  crcm=$(grep -c -E "crc mismatch|bad magic|bad header" "$rlog")
  pan=$(grep -c "panic" "$rlog")

  # 4) fsck
  fl=$(./fsck/fsck -n "$WORK/work.img" 2>&1); fx=$?
  last=$(printf '%s\n' "$fl" | tail -n 1)

  echo "$i,$seed,$CPUS,$POINT,$n,$crashed,$reboot_ok,$rec,$crcm,$pan,$fx,\"$last\"" >> "$CSV"
  printf '[%d/%d] seed=%d n=%d crashed=%d reboot_ok=%d recovering=%d fsck_exit=%d (%s)\n' \
    $((i + 1)) "$RUNS" "$seed" "$n" "$crashed" "$reboot_ok" "$rec" "$fx" "$last"
  if [ $crashed -eq 0 ]; then nocrash=$((nocrash + 1)); elif [ $fx -eq 0 ] && [ $reboot_ok -eq 1 ]; then clean=$((clean + 1)); else bad=$((bad + 1)); fi
done

# tra kernel ve ban sach de lan make sau khong bi crash ngam
touch kernel/crash.c; make -s kernel/kernel >/dev/null 2>&1

echo "[crash_loop] xong: $RUNS lan | crash+clean=$clean | crash+loi=$bad | khong crash=$nocrash | CSV: $CSV"
[ $bad -eq 0 ]
