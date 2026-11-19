#!/usr/bin/env bash
# mayhem/build.sh — builds libsameboy's Core (Core/*.c) twice: sanitized for the fuzz harnesses
# (mayhem/harnesses/fuzz_*.c → /mayhem/<name> + /mayhem/<name>-standalone) and with normal flags
# for the known-answer selftest (mayhem/selftest.c → build/mayhem-test/sameboy_selftest) that
# mayhem/test.sh runs. Upstream's Makefile is not used: its `lib` target needs `cppp` and the
# frontends/boot ROMs need rgbds + SDL; the Core itself is plain C with only libm.
# Outputs live under build/ (gitignored upstream) and /mayhem/fuzz_*; re-runnable and offline.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS
cd "$SRC"

VERSION=$(sed -n 's/^VERSION := //p' version.mk)
YEAR=$(grep -oE "20[2-9][0-9]" LICENSE | head -1)
COMMON=(-std=gnu11 -D_GNU_SOURCE "-DGB_VERSION=\"$VERSION\"" "-DGB_COPYRIGHT_YEAR=\"$YEAR\"" -I. -D_USE_MATH_DEFINES
        -Wno-multichar -Wno-nonnull -Wno-unused-result -Wno-int-in-bool-context -Wno-missing-braces)

# compile_core <outdir> <flags...>: every Core/*.c with -DGB_INTERNAL, in parallel.
compile_core() {
  local out="$1"; shift
  mkdir -p "$out"
  printf '%s\n' Core/*.c | xargs -P "$MAYHEM_JOBS" -I{} sh -c \
    'f="$1"; shift; o="$0/$(basename "$f" .c).o"; exec "$@" -DGB_INTERNAL -c "$f" -o "$o"' \
    "$out" {} "$CC" "${COMMON[@]}" "$@"
}

# Three UBSan checks are relaxed (ASan and the rest of UBSan stay on and halting), each because it
# fired on (nearly) every input on an idiomatic pattern and blocked fuzzing:
#  - nonnull-attribute: memset/memcpy/fread with NULL and length 0 (Core/gb.c:1640
#    memset(gb->mbc_ram, 0xFF, 0) on every reset of a cartridge without RAM).
#  - pointer-overflow: NULL + 0 (Core/save_state.c:1359 gb->mbc_ram + 0 when there is no cart RAM).
#  - shift-base: the CPU core's flag math `(a - value) << 8` on promoted ints (Core/sm83_cpu.c SUB/
#    SBC/CP), executed by any program that subtracts.
SANITIZER_FLAGS="$SANITIZER_FLAGS -fno-sanitize=nonnull-attribute,pointer-overflow,shift-base"

# 1) Sanitized + instrumented Core for the fuzzers.
# shellcheck disable=SC2086
compile_core build/mayhem-fuzz -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.c -o build/mayhem-fuzz-lsan_off.o

# 2) Each harness twice: libFuzzer binary + standalone run-once reproducer.
for h in mayhem/harnesses/fuzz_*.c; do
  name=$(basename "$h" .c)
  # shellcheck disable=SC2086
  $CC "${COMMON[@]}" -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$h" \
      build/mayhem-fuzz/*.o build/mayhem-fuzz-lsan_off.o -lm -o "/mayhem/$name"
  # shellcheck disable=SC2086
  $CC "${COMMON[@]}" -O1 $SANITIZER_FLAGS $DEBUG_FLAGS "$STANDALONE_FUZZ_MAIN" "$h" \
      build/mayhem-fuzz/*.o build/mayhem-fuzz-lsan_off.o -lm -o "/mayhem/$name-standalone"
done

# 3) Functional selftest against a normally-built Core (no sanitizers).
# shellcheck disable=SC2086
compile_core build/mayhem-test -O2 $COVERAGE_FLAGS
# shellcheck disable=SC2086
$CC "${COMMON[@]}" -O2 $COVERAGE_FLAGS mayhem/selftest.c build/mayhem-test/*.o -lm -o build/mayhem-test/sameboy_selftest
echo "build.sh: done"
