#!/usr/bin/env bash
# Automated corrupted-image tests for xv6-26121

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

make -s fsck/fsck fsck/corrupt fs.img

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

check_clean() {
  local result status

  if result="$(./fsck/fsck -n fs.img 2>&1)"; then
    status=0
  else
    status=$?
  fi

  if [[ "$status" -eq 0 ]] &&
     grep -Fq 'fsck: clean' <<< "$result"; then
    pass F01 'clean image'
  else
    fail F01 "clean image (fsck exit=$status)"
    printf '%s\n' "$result"
  fi
}

check_corrupt() {
  local id="$1" mode="$2" expected="$3"
  local image="$WORK/${id}.img"
  local result status checksum_before checksum_after

  cp -- fs.img "$image"

  if ! ./fsck/corrupt "$mode" "$image" \
    > "$WORK/${id}.inject.log" 2>&1; then
    fail "$id" "cannot inject $mode"
    cat "$WORK/${id}.inject.log"
    return
  fi

  checksum_before="$(sha256sum "$image" | cut -d' ' -f1)"

  if result="$(./fsck/fsck -n "$image" 2>&1)"; then
    status=0
  else
    status=$?
  fi

  checksum_after="$(sha256sum "$image" | cut -d' ' -f1)"

  if [[ "$status" -eq 1 ]] &&
     grep -Eq "$expected" <<< "$result" &&
     [[ "$checksum_before" == "$checksum_after" ]]; then
    pass "$id" "$mode"
  else
    fail "$id" "$mode (fsck exit=$status)"

    if [[ "$checksum_before" != "$checksum_after" ]]; then
      echo 'ERROR: fsck -n modified the image'
    fi

    printf '%s\n' "$result"
  fi
}

check_clean
check_corrupt F03 bitmap-clear 'used but marked free in bitmap'
check_corrupt F04 bitmap-set 'marked used in bitmap but not referenced'
check_corrupt F05 nlink-bump 'nlink mismatch'
check_corrupt F06 dup-block 'referenced more than once'
check_corrupt F07 dirent-drop 'nlink mismatch|orphan inode'

printf '\nSummary: %d passed, %d failed\n' "$passed" "$failed"

if (( failed > 0 )); then
  exit 1
fi
