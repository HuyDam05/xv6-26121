#!/usr/bin/env python3
"""Offline fsck repair with a verified temporary image."""

import os
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

FSCK = Path(__file__).resolve().parents[1] / "fsck/fsck"


def main():
    if len(sys.argv) != 2:
        print("Usage: fsck-safe-repair.py IMAGE", file=sys.stderr)
        return 2

    image = Path(sys.argv[1]).absolute()

    if image.name == "fs.img" or image.is_symlink():
        print("REFUSED: fs.img or symbolic link", file=sys.stderr)
        return 2

    original = image.stat()

    if not stat.S_ISREG(original.st_mode) or original.st_nlink != 1:
        print("REFUSED: unsafe image file", file=sys.stderr)
        return 2

    fd, temp = tempfile.mkstemp(
        prefix=".fsck-repair-", suffix=".img",
        dir=image.parent
    )

    try:
        # Create a complete temporary copy on the same filesystem.
        with os.fdopen(fd, "wb") as dst, image.open("rb") as src:
            shutil.copyfileobj(src, dst)
            dst.flush()
            os.fsync(dst.fileno())

        os.chmod(temp, stat.S_IMODE(original.st_mode))

        print("=== REPAIR TEMPORARY IMAGE ===", flush=True)
        repair = subprocess.run([str(FSCK), "-y", temp])
        if repair.returncode != 0:
            print("ABORT: repair failed; original unchanged")
            return repair.returncode

        print("=== VERIFY TEMPORARY IMAGE ===", flush=True)
        verify = subprocess.run([str(FSCK), "-n", temp])
        if verify.returncode != 0:
            print("ABORT: verification failed; original unchanged")
            return verify.returncode

        # Detect ordinary concurrent modifications before replacement.
        latest = image.stat()
        if (
            latest.st_ino != original.st_ino
            or latest.st_size != original.st_size
            or latest.st_mtime_ns != original.st_mtime_ns
        ):
            print("ABORT: original changed during repair")
            return 2

        with open(temp, "rb") as f:
            os.fsync(f.fileno())

        os.replace(temp, image)

        dirfd = os.open(image.parent, os.O_RDONLY)
        try:
            os.fsync(dirfd)
        finally:
            os.close(dirfd)

        print("PASS: verified image installed")
        return 0

    finally:
        if os.path.exists(temp):
            os.unlink(temp)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except OSError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(2)
