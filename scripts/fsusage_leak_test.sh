
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CPUS="${CPUS:-1}"

if [[ "$CPUS" != "1" && "$CPUS" != "4" ]]; then
  echo "ERROR: CPUS must be 1 or 4"
  exit 2
fi

make -s kernel/kernel fs.img fsck/fsck

WORK="$(mktemp -d)"
trap 'rm -rf -- "$WORK"' EXIT

IMAGE="$WORK/n15-test.img"
cp fs.img "$IMAGE"

EVIDENCE="docs/bao-cao/W05/huy"
mkdir -p "$EVIDENCE"
LOG="$EVIDENCE/n15-cpus${CPUS}.log"

echo "N15: testing with CPUS=$CPUS"

set +e
timeout --signal=TERM --kill-after=2s 50s \
  qemu-system-riscv64 \
    -machine virt -bios none \
    -kernel kernel/kernel -m 128M \
    -smp "$CPUS" -nographic \
    -global virtio-mmio.force-legacy=false \
    -drive "file=$IMAGE,if=none,format=raw,id=x0" \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
    < <(
      sleep 4
      printf 'fsusagetest\nfsleaktest\n'
      sleep 30
      printf '\001x'
    ) > "$LOG" 2>&1
QEMU_STATUS=$?
set -e

if [[ "$QEMU_STATUS" -ne 0 &&
      "$QEMU_STATUS" -ne 124 ]]; then
  echo "FAIL: QEMU returned $QEMU_STATUS"
  tail -n 40 "$LOG"
  exit 1
fi

if ! grep -Fq "fsusagetest: ALL TESTS PASSED" "$LOG" ||
   ! grep -Fq "N15 RESULT: PASS" "$LOG" ||
   grep -qi "panic" "$LOG"; then
  echo "FAIL: guest tests incomplete or failed"
  tail -n 40 "$LOG"
  exit 1
fi

if ! ./fsck/fsck -n "$IMAGE" |
     grep -Fq "fsck: clean"; then
  echo "FAIL: filesystem not clean"
  exit 1
fi

echo "PASS N15 CPUS=$CPUS: no block/inode leak"
echo "PASS: fsck -n clean"
echo "Evidence: $LOG"
