#!/usr/bin/env bash
# Run the Python PTY end-to-end suite against build/term-ime.
#
# One list, three callers: ci.yml gates PRs with it, release.yml runs it in the
# tests job before publishing, and the local pre-release gate runs the same
# script. It used to be inlined in ci.yml only, which is how a release could
# (and did) ship without any e2e evidence -- install.sh points at
# /releases/latest, so an untested artifact is immediately what everyone gets.
#
# Flake policy (2026-10-13): a failed suite is retried ONCE. Passing on the
# retry still prints it as FLAKY in the summary -- the retry bounds the damage,
# it does not erase the event (root cause of the test_settings_panel_e2e CI
# flake is unidentified: the child dies mid-panel-open on the runner and never
# reproduces locally; the suite now prints the poll stop-reason and the child's
# exit status so the next occurrence names its own cause). Two consecutive
# failures fail the run -- 连挂两次才当真缺陷查. TERM_IME_E2E_RETRY=0 restores
# strict single-attempt runs.
set -euo pipefail

cd "$(dirname "$0")/.."

if [ ! -x build/term-ime ]; then
  echo "ERROR: build/term-ime not found -- configure and build first" >&2
  exit 1
fi

attempts=1
if [ "${TERM_IME_E2E_RETRY:-1}" != "0" ]; then
  attempts=2
fi

failed=()
flaked=()

for suite in \
  test_settings_panel_e2e \
  test_settings_e2e \
  test_e2e \
  test_simplified_candidates \
  test_candidate_paging \
  test_candidate_paging_hint \
  test_wide_pair \
  test_config_types_e2e \
  test_fuzzy_pinyin \
  test_punctuation \
  test_paste_delivery
do
  echo ">> $suite"
  if python3 "tests/${suite}.py"; then
    continue
  fi
  if [ "$attempts" -ge 2 ]; then
    echo "!! $suite FAILED -- retrying once (flake guard; see brain e2e-harness-contract)"
    if python3 "tests/${suite}.py"; then
      flaked+=("$suite")
      continue
    fi
  fi
  failed+=("$suite")
done

echo ""
echo "== e2e summary: ${#failed[@]} failed, ${#flaked[@]} flaked-recovered =="
if [ "${#flaked[@]}" -gt 0 ]; then
  echo "FLAKY (passed only on retry -- diagnose, don't let it become normal): ${flaked[*]}"
fi
if [ "${#failed[@]}" -gt 0 ]; then
  echo "FAILED (after retry where enabled): ${failed[*]}"
  exit 1
fi
