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


check_bad_superblock() {
  local id="$1" offset="$2" expected="$3"
  local image="$WORK/${id}.img"
  local result status before after

  cp -- fs.img "$image"

  # Overwrite one 32-bit superblock field in the copy.
  printf '\0\0\0\0' |
    dd of="$image" bs=1 seek="$((1024 + offset))" \
       conv=notrunc status=none

  before="$(sha256sum "$image" | cut -d' ' -f1)"

  if result="$(./fsck/fsck -n "$image" 2>&1)"; then
    status=0
  else
    status=$?
  fi

  after="$(sha256sum "$image" | cut -d' ' -f1)"

  if [[ "$status" -eq 1 ]] &&
     grep -Fq "$expected" <<< "$result" &&
     [[ "$before" == "$after" ]]; then
    pass "$id" 'superblock corruption detected'
  else
    fail "$id" "superblock check (exit=$status)"
    printf '%s\n' "$result"
  fi
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

check_cli_error() {
  local id="$1" expected="$2"
  shift 2
  local result status

  if result="$(./fsck/fsck "$@" 2>&1)"; then
    status=0
  else
    status=$?
  fi

  if [[ "$status" -eq 2 ]] &&
     grep -Fq "$expected" <<< "$result"; then
    pass "$id" 'invalid argument rejected'
  else
    fail "$id" "wrong exit/message (exit=$status)"
    printf '%s\n' "$result"
  fi
}

check_readonly_clean() {
  local image="$WORK/F12.img"
  local before after result status

  cp -- fs.img "$image"
  before="$(sha256sum "$image" | cut -d' ' -f1)"

  if result="$(./fsck/fsck -n "$image" 2>&1)"; then
    status=0
  else
    status=$?
  fi

  after="$(sha256sum "$image" | cut -d' ' -f1)"

  if [[ "$status" -eq 0 ]] &&
     grep -Fq 'fsck: clean' <<< "$result" &&
     [[ "$before" == "$after" ]]; then
    pass F12 'read-only image unchanged'
  else
    fail F12 'read-only verification'
    printf '%s\n' "$result"
  fi
}

check_repair() {
  local id="$1" mode="$2" expected="$3"
  local image="$WORK/${id}.img"
  local detected repaired verified
  local detect_status repair_status verify_status

  cp -- fs.img "$image"

  if ! ./fsck/corrupt "$mode" "$image" \
       > "$WORK/${id}.inject.log" 2>&1; then
    fail "$id" "cannot inject $mode"
    return
  fi

  if detected="$(./fsck/fsck -n "$image" 2>&1)"; then
    detect_status=0
  else
    detect_status=$?
  fi

  if repaired="$(./fsck/fsck -y "$image" 2>&1)"; then
    repair_status=0
  else
    repair_status=$?
  fi

  if verified="$(./fsck/fsck -n "$image" 2>&1)"; then
    verify_status=0
  else
    verify_status=$?
  fi

  if [[ "$detect_status" -eq 1 ]] &&
     grep -Fq "$expected" <<< "$detected" &&
     [[ "$repair_status" -eq 0 ]] &&
     grep -Fq 'fsck: clean' <<< "$repaired" &&
     [[ "$verify_status" -eq 0 ]] &&
     grep -Fq 'fsck: clean' <<< "$verified"; then
    pass "$id" "$mode repaired and verified"
  else
    fail "$id" "$mode repair failed"
    printf 'Detect:\n%s\n' "$detected"
    printf 'Repair:\n%s\n' "$repaired"
    printf 'Verify:\n%s\n' "$verified"
  fi
}


check_clean
check_bad_superblock F02a 0 'bad magic'
check_bad_superblock F02b 4 'does not match image size'
check_corrupt F03 bitmap-clear 'used but marked free in bitmap'
check_corrupt F04 bitmap-set 'marked used in bitmap but not referenced'
check_corrupt F05 nlink-bump 'nlink mismatch'
check_corrupt F06 dup-block 'referenced more than once'
check_corrupt F07 dirent-drop 'nlink mismatch|orphan inode'
check_readonly_clean

check_cli_error F13a 'usage:'
check_cli_error F13b 'usage:' -z fs.img
check_cli_error F13c 'missing.img' -n "$WORK/missing.img"

check_repair Y01 bitmap-clear 'used but marked free in bitmap'
check_repair Y02 bitmap-set 'marked used in bitmap but not referenced'
check_repair Y03 nlink-bump 'nlink mismatch'

printf '\nSummary: %d passed, %d failed\n' "$passed" "$failed"

if (( failed > 0 )); then
  exit 1
fi
