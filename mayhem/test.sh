#!/usr/bin/env bash
#
# mayhem/test.sh — run the libsais behavioral self-test suite built by
# mayhem/build.sh. Upstream ships no test suite of its own, so the oracle is
# mayhem/libsais_selftest.c: every libsais result (SA, LCP, BWT, 16/64-bit
# variants) is asserted against an independent brute-force reference and
# known answers. Emits a CTRF report.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
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

RUNNER=/mayhem/libsais_selftest
if [ ! -x "$RUNNER" ]; then
  echo "FATAL: $RUNNER missing — mayhem/build.sh must build it" >&2
  emit_ctrf "libsais-selftest" 0 1
  exit 1
fi

OUT="$("$RUNNER" 2>&1)"; rc=$?
echo "$OUT"

SUMMARY="$(echo "$OUT" | grep -E '^RESULTS: total=[0-9]+ passed=[0-9]+ failed=[0-9]+$' | tail -1)"
if [ -z "$SUMMARY" ]; then
  echo "FATAL: self-test produced no RESULTS summary (rc=$rc)" >&2
  emit_ctrf "libsais-selftest" 0 1
  exit 1
fi

passed="$(echo "$SUMMARY" | sed -E 's/.*passed=([0-9]+).*/\1/')"
failed="$(echo "$SUMMARY" | sed -E 's/.*failed=([0-9]+).*/\1/')"
emit_ctrf "libsais-selftest" "$passed" "$failed"
