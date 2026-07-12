#!/usr/bin/env bash
#
# mayhem/build.sh — libsais commit-image build.
#
# Builds (a) the library with $SANITIZER_FLAGS + $DEBUG_FLAGS and the sais-fuzz
# harness (fuzzer + standalone reproducer) against it, and (b) a clean
# normal-flags library plus the behavioral self-test suite that mayhem/test.sh
# runs. Air-gapped: cmake + clang only, no network. Idempotent: safe to re-run.
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

# 1) Sanitized + DWARF-3 instrumented library (the fuzzed code)
cmake -B build -DCMAKE_C_COMPILER="$CC" \
      -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$MAYHEM_JOBS"

# 2) Harness: libFuzzer binary + standalone run-once reproducer
$CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
    "$SRC/mayhem/sais-fuzz.c" -I"$SRC/include" "$SRC/build/libsais.a" \
    -o /mayhem/sais-fuzz
$CC $SANITIZER_FLAGS $DEBUG_FLAGS "$STANDALONE_FUZZ_MAIN" \
    "$SRC/mayhem/sais-fuzz.c" -I"$SRC/include" "$SRC/build/libsais.a" \
    -o /mayhem/sais-fuzz-standalone

# 3) Test-suite build with the project's NORMAL flags (independent clean build);
#    mayhem/test.sh only RUNS /mayhem/libsais_selftest.
cmake -B build-tests -DCMAKE_C_COMPILER="$CC" \
      -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests -j"$MAYHEM_JOBS"
$CC -O2 $COVERAGE_FLAGS "$SRC/mayhem/libsais_selftest.c" -I"$SRC/include" \
    "$SRC/build-tests/libsais.a" -o /mayhem/libsais_selftest

echo "build.sh: done"
