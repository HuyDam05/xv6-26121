#!/usr/bin/env bash
# xv6-26121 / Hung W04 (N12): kiem thu cac nhanh recovery cua log.
#
# Y tuong: tao anh that su co log cho recovery (crash o diem 2: da commit,
# chua install), sao chep roi sua dung header hoac mot byte log block,
# boot lai tren chinh ban sua va kiem tra kernel + fsck -n.
#
#   L00  doi chung  : anh crash nguyen ven  -> phai replay ("recovering tail")
#   L01  log-flip   : sai 1 byte log block  -> "crc mismatch, dropping log"
#   L02  log-magic  : sai magic             -> "bad magic, dropping log"
#   L03  log-n      : n ngoai khoang        -> "bad header n=..., dropping log"
#   L04  crash_ctl  : tham so sai           -> "invalid argument", khong crash
#
# Moi ca kiem tra: thong bao dung, khong panic, shell len, fsck -n clean.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CPUS="${CPUS:-1}"
BOOT_DELAY="${BOOT_DELAY:-3}"
CRASH_TIMEOUT="${CRASH_TIMEOUT:-45}"
REBOOT_TIMEOUT="${REBOOT_TIMEOUT:-9}"

# Bo cuc dia: block 2 = log header, block 3.. = log block dau tien.
# logheader: n (4 B) + block[30] (120 B) + magic (4 B) + crc (4 B).
HDR=$((2 * 1024))
LOG0=$((3 * 1024))
MAGIC_OFF=$((HDR + 124))

for tool in qemu-system-riscv64 timeout make xxd dd; do
  command -v "$tool" > /dev/null || {
    echo "Missing: $tool" >&2
    exit 2
  }
done

make -s kernel/kernel fs.img fsck/fsck

WORK="$(mktemp -d)"
trap 'rm -rf -- "$WORK"' EXIT

passed=0
failed=0

pass() {
  printf 'PASS %s: %s\n' "$1" "$2"
  passed=$((passed + 1))
}

fail() {
  printf 'FAIL %s: %s\n' "$1" "$2"
  failed=$((failed + 1))
}

qemu_run() {
  local seconds="$1" image="$2" command="$3" output="$4"

  timeout --signal=TERM --kill-after=2s "$seconds" \
    qemu-system-riscv64 -machine virt -bios none \
    -kernel kernel/kernel -m 128M -smp "$CPUS" -nographic \
    -global virtio-mmio.force-legacy=false \
    -drive "file=$image,if=none,format=raw,id=x0" \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
    < <(sleep "$BOOT_DELAY"; printf '%s\n' "$command"; sleep "$seconds") \
    > "$output" 2>&1 || true
}

byte_at() {
  xxd -s "$2" -l 1 -p "$1"
}

# Dao toan bo bit cua 1 byte tai offset.
flip_byte() {
  local image="$1" off="$2" old new
  old="$(byte_at "$image" "$off")"
  new="$(printf '%02x' $((0x$old ^ 0xff)))"
  printf "\x$new" | dd of="$image" bs=1 seek="$off" conv=notrunc status=none
}

# Dat n = 255 (> LOGBLOCKS = 30), little-endian.
set_bad_n() {
  printf '\xff\x00\x00\x00' | dd of="$1" bs=1 seek="$HDR" conv=notrunc status=none
}

header_n() {
  local hex
  hex="$(xxd -s "$HDR" -l 4 -p "$1")"
  echo $((0x${hex:6:2}${hex:4:2}${hex:2:2}${hex:0:2}))
}

magic_ok() {
  [[ "$(xxd -s "$MAGIC_OFF" -l 4 -p "$1")" == "43474f4c" ]]
}

# Boot lai tren anh, gui lenh 'ls', kiem tra ket qua.
#   $4 = regex thong bao mong doi ('' neu khong can)
#   $5 = 1 neu mong doi replay (recovering tail), 0 neu mong doi bo log
boot_check() {
  local id="$1" desc="$2" image="$3" expected="$4" want_replay="$5"
  local log="$WORK/$id.boot.log" why="" replays fsck_out fsck_status

  qemu_run "$REBOOT_TIMEOUT" "$image" 'ls' "$log"

  if [[ -n "$expected" ]] && ! grep -Eq "$expected" "$log"; then
    why+=" thieu-thong-bao"
  fi

  replays="$(grep -c 'recovering tail' "$log" || true)"
  if [[ "$want_replay" == 1 && "$replays" -eq 0 ]]; then
    why+=" khong-replay"
  fi
  if [[ "$want_replay" == 0 && "$replays" -ne 0 ]]; then
    why+=" replay-rac"
  fi

  if grep -qi 'panic' "$log"; then
    why+=" panic"
  fi
  if ! grep -Fq 'init: starting sh' "$log" || ! grep -Fq 'README' "$log"; then
    why+=" khong-len-shell"
  fi

  # ct0 la file do transaction crash dang tao: co khi replay, khong khi bo log.
  if [[ "$want_replay" == 1 ]] && ! grep -Fq 'ct0' "$log"; then
    why+=" thieu-ct0"
  fi
  if [[ "$want_replay" == 0 ]] && grep -Fq 'ct0' "$log"; then
    why+=" co-ct0"
  fi

  if fsck_out="$(./fsck/fsck -n "$image" 2>&1)"; then
    fsck_status=0
  else
    fsck_status=$?
  fi
  if [[ "$fsck_status" -ne 0 ]] || ! grep -Fq 'fsck: clean' <<< "$fsck_out"; then
    why+=" fsck-khong-clean"
  fi

  if [[ -z "$why" ]]; then
    pass "$id" "$desc"
  else
    fail "$id" "$desc [$why ]"
    grep -E 'log:|recovering|panic|ct0' "$log" || true
    printf '%s\n' "$fsck_out"
    cp -- "$log" "results/${id}.boot.log" 2> /dev/null || true
  fi
}

# --- 1. Tao anh crash o diem 2 va kiem tra anh dung la anh can ---------------
CRASH_IMG="$WORK/crash2.img"
cp -- fs.img "$CRASH_IMG"
qemu_run "$CRASH_TIMEOUT" "$CRASH_IMG" 'crashtest 2 1' "$WORK/crash2.log"

if ! grep -Fq 'CRASH: point=2' "$WORK/crash2.log"; then
  echo 'ERROR: khong crash duoc o diem 2, khong co anh de test' >&2
  exit 2
fi

n="$(header_n "$CRASH_IMG")"
if (( n <= 0 )) || ! magic_ok "$CRASH_IMG"; then
  echo "ERROR: anh crash khong co log cho recovery (n=$n)" >&2
  exit 2
fi
echo "Anh crash diem 2: n=$n, magic hop le"

# --- 2. Cac ca recovery -------------------------------------------------------
cp -- "$CRASH_IMG" "$WORK/L00.img"
boot_check L00 'doi chung: anh crash nguyen ven, phai replay' \
  "$WORK/L00.img" '' 1

cp -- "$CRASH_IMG" "$WORK/L01.img"
flip_byte "$WORK/L01.img" $((LOG0 + 10))
boot_check L01 'log-flip: sai 1 byte log block' \
  "$WORK/L01.img" 'log: crc mismatch, dropping log' 0

cp -- "$CRASH_IMG" "$WORK/L02.img"
flip_byte "$WORK/L02.img" "$MAGIC_OFF"
boot_check L02 'log-magic: sai magic' \
  "$WORK/L02.img" 'log: bad magic, dropping log' 0

cp -- "$CRASH_IMG" "$WORK/L03.img"
set_bad_n "$WORK/L03.img"
boot_check L03 'log-n: n ngoai khoang' \
  "$WORK/L03.img" 'log: bad header n=255, dropping log' 0

# --- 3. crash_ctl voi tham so sai: bao loi, khong crash -----------------------
cp -- fs.img "$WORK/L04.img"
qemu_run "$REBOOT_TIMEOUT" "$WORK/L04.img" \
  $'crashtest 99 1\ncrashtest 6 1' "$WORK/L04.boot.log"

invalid="$(grep -c 'invalid argument' "$WORK/L04.boot.log" || true)"
if [[ "$invalid" -ge 2 ]] &&
   ! grep -Fq 'CRASH:' "$WORK/L04.boot.log" &&
   ! grep -qi 'panic' "$WORK/L04.boot.log" &&
   ./fsck/fsck -n "$WORK/L04.img" | grep -Fq 'fsck: clean'; then
  pass L04 'crash_ctl tham so sai: bao loi, khong crash'
else
  fail L04 "crash_ctl tham so sai (invalid argument=$invalid)"
  tail -n 15 "$WORK/L04.boot.log"
fi

printf '\nSummary: %d passed, %d failed\n' "$passed" "$failed"

if (( failed > 0 )); then
  exit 1
fi
