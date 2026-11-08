#!/usr/bin/env bash
# xv6-26121 / Huy W03: crash -> reboot -> fsck, using crash_ctl.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

RUNS="${1:-10}"
SEED_BASE="${SEED_BASE:-101}"
CPUS="${CPUS:-1}"
BOOT_DELAY="${BOOT_DELAY:-3}"
CRASH_TIMEOUT="${CRASH_TIMEOUT:-45}"
REBOOT_TIMEOUT="${REBOOT_TIMEOUT:-9}"
WORK="results/work-v2"
CSV="results/crash_runs_after_crc.csv"

for name in RUNS SEED_BASE CPUS; do
  value="${!name}"
  if ! [[ "$value" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: $name must be a positive integer" >&2
    exit 2
  fi
done

for tool in qemu-system-riscv64 timeout make; do
  command -v "$tool" >/dev/null || {
    echo "Missing: $tool" >&2
    exit 2
  }
done

mkdir -p "$WORK/logs" results
make -s kernel/kernel fs.img fsck/fsck

if ! ./fsck/fsck -n fs.img | grep -Fq 'fsck: clean'; then
  echo 'ERROR: original fs.img is not clean' >&2
  exit 2
fi

cp fs.img "$WORK/fs.base.img"

printf '%s\n' \
'run,seed,cpus,point,n,crashed,reboot_ok,recovering_lines,crc_msgs,panic,fsck_exit,fsck_last_line,status' > "$CSV"

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

pass=0
fail=0
points=(1 2 3 4 5)

for ((i = 0; i < RUNS; i++)); do
  seed=$((SEED_BASE + i))
  point="${POINT:-${points[$(((seed - 1) % 5))]}}"
  n="${COUNT:-$((1 + (seed * 7) % 3))}"
  tag="s${seed}_p${point}_n${n}_c${CPUS}"

  image="$WORK/current.img"
  crashlog="$WORK/logs/${tag}.crash.log"
  rebootlog="$WORK/logs/${tag}.reboot.log"

  cp "$WORK/fs.base.img" "$image"

  qemu_run "$CRASH_TIMEOUT" "$image" \
    "crashtest $point $n" "$crashlog"

  crashed=0
  if grep -Fq "CRASH: point=$point" "$crashlog"; then
    crashed=1
  fi

  qemu_run "$REBOOT_TIMEOUT" "$image" 'ls' "$rebootlog"

  reboot_ok=0
  if grep -Fq 'init: starting sh' "$rebootlog" &&
     grep -Fq 'README' "$rebootlog"; then
    reboot_ok=1
  fi

  recovering="$(grep -c 'recovering tail' "$rebootlog" || true)"
  crc_msgs="$(grep -Ec 'crc mismatch|bad magic|bad header' "$rebootlog" || true)"
  panic="$(grep -ci 'panic' "$rebootlog" || true)"

  if fsck_output="$(./fsck/fsck -n "$image" 2>&1)"; then
    fsck_exit=0
  else
    fsck_exit=$?
  fi

  last_line="$(printf '%s\n' "$fsck_output" | tail -n 1 | tr -d '\r')"
  last_line="${last_line//\"/\"\"}"

  status=FAIL

  if ((crashed == 1 && reboot_ok == 1 && panic == 0 && fsck_exit == 0)) &&
     grep -Fq 'fsck: clean' <<< "$fsck_output"; then
    status=PASS
    pass=$((pass + 1))
  else
    fail=$((fail + 1))
  fi

  printf '%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"%s",%s\n' \
    "$i" "$seed" "$CPUS" "$point" "$n" "$crashed" "$reboot_ok" \
    "$recovering" "$crc_msgs" "$panic" "$fsck_exit" \
    "$last_line" "$status" >> "$CSV"

  printf '[%d/%d] %s seed=%d point=%d n=%d crash=%d reboot=%d fsck=%d (%s)\n' \
    "$((i + 1))" "$RUNS" "$status" "$seed" "$point" "$n" \
    "$crashed" "$reboot_ok" "$fsck_exit" "$last_line"
done

printf '[crash_loop] xong: %d lan | crash+clean=%d | fail=%d | CSV: %s\n' \
  "$RUNS" "$pass" "$fail" "$CSV"

((fail == 0))