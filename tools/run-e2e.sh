#!/usr/bin/env bash
# Run the Python PTY end-to-end suite against build/term-ime.
#
# One list, two callers: ci.yml runs it on every push and release.yml runs it
# before publishing. It used to be inlined in ci.yml only, which is how a release
# could (and did) ship without any e2e evidence -- install.sh points at
# /releases/latest, so an untested artifact is immediately what everyone gets.
set -euo pipefail

cd "$(dirname "$0")/.."

if [ ! -x build/term-ime ]; then
  echo "ERROR: build/term-ime not found -- configure and build first" >&2
  exit 1
fi

for suite in \
  test_settings_panel_e2e \
  test_settings_e2e \
  test_e2e \
  test_simplified_candidates \
  test_candidate_paging \
  test_wide_pair \
  test_config_types_e2e \
  test_fuzzy_pinyin \
  test_punctuation \
  test_paste_delivery
do
  echo ">> $suite"
  python3 "tests/${suite}.py"
done
