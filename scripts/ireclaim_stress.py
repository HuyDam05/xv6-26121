#!/usr/bin/env python3

import argparse
import hashlib
import re
import shutil
import signal
import subprocess
import tempfile
from pathlib import Path

import pexpect

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/bao-cao/W06/VuTienDuc"
TIMEOUT = 120


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def boot(image, cpus, log):
    args = [
        "-machine", "virt",
        "-bios", "none",
        "-kernel", str(ROOT / "kernel/kernel"),
        "-m", "128M",
        "-smp", str(cpus),
        "-nographic",
        "-global", "virtio-mmio.force-legacy=false",
        "-drive", f"file={image},if=none,format=raw,id=x0",
        "-device", "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0",
    ]

    qemu = pexpect.spawn(
        "qemu-system-riscv64",
        args,
        encoding="utf-8",
        timeout=TIMEOUT,
    )

    qemu.logfile = log
    return qemu


def stop(qemu):
    if qemu is not None:
        if qemu.isalive():
            qemu.kill(signal.SIGKILL)
        qemu.close(force=True)


def expect(qemu, pattern):
    result = qemu.expect([
        pattern,
        r"panic:",
        r"FAIL[^\r\n]*\r?\n",
        pexpect.EOF,
        pexpect.TIMEOUT,
    ])

    if result != 0:
        raise RuntimeError(f"Unexpected QEMU result: {result}; before={qemu.before!r}; after={qemu.after!r}")


def fsck(image, log):
    before = sha256(image)

    result = subprocess.run(
        [str(ROOT / "fsck/fsck"), "-n", str(image)],
        capture_output=True,
        text=True,
        timeout=60,
    )

    output = result.stdout + result.stderr
    log.write(output)
    log.flush()

    if sha256(image) != before:
        raise RuntimeError("fsck -n modified image")

    if result.returncode != 0 or "fsck: clean" not in output:
        raise RuntimeError("fsck verification failed")

    return output


def run_once(cpus, run, work):
    image = work / f"cpus{cpus}-run{run:02d}.img"
    shutil.copyfile(ROOT / "fs.img", image)

    logfile = EVIDENCE / f"cpus{cpus}-run{run:02d}.log"
    qemu = None

    with logfile.open("w") as log:
        log.write(f"CPUS={cpus} RUN={run}\n")

        try:
            log.write("\n=== PREPARE ===\n")
            qemu = boot(image, cpus, log)

            expect(qemu, r"\$ ")
            qemu.sendline("ireclaimtest prepare")

            expect(
                qemu,
                r"IRECLAIM_READY baseline_blocks=(\d+) baseline_inodes=(\d+)\r?\n"
            )

            blocks = int(qemu.match.group(1))
            inodes = int(qemu.match.group(2))

            log.write("\n=== CRASH: SIGKILL ===\n")
            stop(qemu)
            qemu = None

            log.write("\n=== BEFORE REBOOT FSCK ===\n")
            output = fsck(image, log)

            pending = re.findall(
                r"WARNING: inode (\d+) has nlink=0",
                output
            )

            if len(set(pending)) != 8:
                raise RuntimeError("Expected exactly 8 pending orphans")

            log.write("\n=== REBOOT ===\n")
            qemu = boot(image, cpus, log)
            expect(qemu, r"\$ ")

            reclaimed = re.findall(
                r"ireclaim: orphaned inode (\d+)",
                qemu.before
            )

            if set(reclaimed) != set(pending):
                raise RuntimeError("Reclaimed inode set mismatch")

            qemu.sendline(
                f"ireclaimtest verify {blocks} {inodes}"
            )

            expect(
                qemu,
                r"PASS stress: all 8 orphan files reclaimed"
            )

            expect(qemu, r"\$ ")

            log.write("\n=== POST-RECOVERY FSCK ===\n")
            stop(qemu)
            qemu = None

            output = fsck(image, log)

            if "WARNING:" in output:
                raise RuntimeError("Warnings remain after reboot")

            log.write("\nPASS: complete crash/reboot cycle\n")

            return True

        except Exception as exc:
            log.write(f"\nFAIL: {exc}\n")
            return False

        finally:
            stop(qemu)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cpus", type=int, choices=[1, 4],
                        required=True)
    parser.add_argument("--runs", type=int, default=10)
    args = parser.parse_args()

    if args.runs < 1:
        parser.error("--runs must be positive")

    EVIDENCE.mkdir(parents=True, exist_ok=True)

    passed = 0

    with tempfile.TemporaryDirectory(
        prefix="duc-w06-ireclaim-"
    ) as tmp:

        for run in range(1, args.runs + 1):
            ok = run_once(args.cpus, run, Path(tmp))
            passed += int(ok)

            status = "PASS" if ok else "FAIL"

            print(
                f"CPUS={args.cpus} "
                f"RUN={run:02d}/{args.runs:02d} "
                f"{status}",
                flush=True,
            )

    failed = args.runs - passed

    print(f"Summary: {passed} passed, {failed} failed")

    return int(failed != 0)


if __name__ == "__main__":
    raise SystemExit(main())
