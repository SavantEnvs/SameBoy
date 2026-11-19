#!/usr/bin/env bash
# mayhem/test.sh — runs the known-answer selftest built by mayhem/build.sh against libsameboy's
# Core (ROM execution, save-state and battery round trips, debugger expressions/commands,
# symbols, cheats, cheat search, GBS header parsing, a DMG frame hash) and compares each
# `name=value` line with mayhem/selftest.expected. One CTRF test per expected line.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

bin=build/mayhem-test/sameboy_selftest
if [ ! -x "$bin" ]; then
  echo "test.sh: $bin missing — run mayhem/build.sh first" >&2
  emit_ctrf sameboy-selftest 0 1
  exit 1
fi
out=$("$bin" 2>&1); rc=$?
passed=0; failed=0
while IFS= read -r want; do
  [ -n "$want" ] || continue
  key=${want%%=*}
  got=$(printf '%s\n' "$out" | grep -m1 "^$key=" || true)
  if [ "$got" = "$want" ]; then passed=$((passed+1)); else failed=$((failed+1)); echo "FAIL $key: want '$want' got '$got'"; fi
done < mayhem/selftest.expected
[ "$rc" -eq 0 ] || { echo "selftest exited $rc"; failed=$((failed+1)); }
emit_ctrf sameboy-selftest "$passed" "$failed"
