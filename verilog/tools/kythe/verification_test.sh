#!/usr/bin/env bash
#
# Copyright 2020 Google LLC.
# SPDX-License-Identifier: Apache-2.0
#
# Extract Kythe indexing facts from SystemVerilog code and check the Kythe
# verification expectations from the annotations.

# --- begin runfiles.bash initialization ---
# Copy-pasted from Bazel's Bash runfiles library (tools/bash/runfiles/runfiles.bash).
set -euo pipefail
if [[ ! -d "${RUNFILES_DIR:-/dev/null}" && ! -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
  if [[ -f "$TEST_SRCDIR/MANIFEST" ]]; then
    export RUNFILES_MANIFEST_FILE="$TEST_SRCDIR/MANIFEST"
  elif [[ -f "$0.runfiles/MANIFEST" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles/MANIFEST"
  elif [[ -f "$TEST_SRCDIR/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
    export RUNFILES_DIR="$TEST_SRCDIR"
  fi
fi
if [[ -f "${RUNFILES_DIR:-/dev/null}/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
  source "${RUNFILES_DIR}/bazel_tools/tools/bash/runfiles/runfiles.bash"
elif [[ -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
  source "$(grep -m1 "^bazel_tools/tools/bash/runfiles/runfiles.bash " \
    "$RUNFILES_MANIFEST_FILE" | cut -d ' ' -f 2-)"
else
  echo >&2 "ERROR: cannot find @bazel_tools//tools/bash/runfiles:runfiles.bash"
  exit 1
fi
# --- end runfiles.bash initialization ---

TESTS_DIR="$(rlocation "com_google_verible/verilog/tools/kythe/testdata")" ||
  {
    echo "Can't load the test data!" >&2
    exit 1
  }
VERIBLE_EXTRACTOR_BIN="$(rlocation "com_google_verible/verilog/tools/kythe/verible-verilog-kythe-extractor")" ||
  {
    echo "Can't load the extractor binary!" >&2
    exit 1
  }
KYTHE_VERIFIER_BIN="$(rlocation "$1")" ||
  {
    echo "Can't load the verifier binary!" >&2
    exit 1
  }

function fail {
  echo "[ERROR] $1"
  exit 1
}

function new_test {
  echo "=== Test '$1'."
  rm -rf "${TEST_TMPDIR}/*"
}

################################################################################
new_test "single files"
test_count=0
for verilog_file in $(ls -d "${TESTS_DIR}"/*); do
  if [[ -d "${verilog_file}" ]]; then
    continue
  fi
  test_filename="$(basename "${verilog_file}")"
  test_dir="${TEST_TMPDIR}/${test_filename%.*}"
  mkdir -p "${test_dir}"
  cp "${verilog_file}" "${test_dir}"
  filelist_path="${test_dir}/filelist"
  echo "${test_filename}" > "${filelist_path}"

  echo "Running Kythe verification test for ${test_filename}"
  "${VERIBLE_EXTRACTOR_BIN}" --file_list_path "${filelist_path}" --file_list_root "${test_dir}" --print_kythe_facts proto  > "${test_dir}/entries" ||
    fail "Failed to extract Kythe facts"
  echo "Extracted.  Now verifying."
  cat "${test_dir}/entries" | "${KYTHE_VERIFIER_BIN}" --nocheck_for_singletons "${test_dir}/${test_filename}" ||
    fail "Verification failed for ${test_filename}"
  test_count=$((${test_count} + 1))
done
[[ ${test_count} -gt 0 ]] || fail "No tests are executed!"


################################################################################
new_test "multi files"
test_case_dir="${TESTS_DIR}/multi_file_test"
test_name="$(basename "${test_case_dir}")"
test_dir="${TEST_TMPDIR}/${test_name}"
mkdir -p "${test_dir}"
cp "${test_case_dir}"/* "${test_dir}/"
filelist_path="${test_dir}/filelist"
ls "${test_case_dir}" > "${filelist_path}"
# Note: file_list.txt from the original test_case_dir seems unused.
echo "Running Kythe verification 'multi file' test for ${test_name}"
"${VERIBLE_EXTRACTOR_BIN}" --file_list_path "${filelist_path}" --file_list_root "${test_dir}" --print_kythe_facts proto  > "${test_dir}/entries" ||
    fail "Failed to extract Kythe facts"
echo "Extracted.  Now verifying."
cat "${test_dir}/entries" | "${KYTHE_VERIFIER_BIN}" "${test_dir}"/*.sv ||
  fail "Verification failed for ${test_name}"


################################################################################
new_test "multi files with include"
test_case_dir="${TESTS_DIR}/include_file_test"
test_name="$(basename "${test_case_dir}")"
test_dir="${TEST_TMPDIR}/${test_name}"
mkdir -p "${test_dir}"
cp "${test_case_dir}"/* "${test_dir}/"
filelist_path="${test_dir}/file_list.txt"
echo "Running Kythe verification 'multi file with include' test for ${test_name}"
"${VERIBLE_EXTRACTOR_BIN}" --include_dir_paths "${test_dir}" --file_list_path "${filelist_path}" --file_list_root "${test_dir}" --print_kythe_facts proto > "${test_dir}/entries" ||
    fail "Failed to extract Kythe facts"
echo "Extracted.  Now verifying."
cat "${test_dir}/entries" | "${KYTHE_VERIFIER_BIN}" "${test_dir}"/*.sv* ||
  fail "Verification failed for ${test_name}"


################################################################################
new_test "multi files with include dir"
test_case_dir="${TESTS_DIR}/include_with_dir_test"
test_name="$(basename "${test_case_dir}")"
test_dir="${TEST_TMPDIR}/${test_name}"
mkdir -p "${test_dir}"
cp -r "${test_case_dir}"/* "${test_dir}/"
filelist_path="${test_dir}/file_list.txt"

first_included="${TESTS_DIR}/include_file_test"
first_included_name="$(basename "${first_included}")"
first_include_dir="${TEST_TMPDIR}/${first_included_name}"
mkdir -p "${first_include_dir}"
cp "${first_included}"/* "${first_include_dir}/"

second_include_dir="${test_dir}/include_dir"
VERILOG_INCLUDE_DIR_TEST_FILES="${test_dir}/*.sv ${first_include_dir}/A.svh ${first_include_dir}/B.svh ${second_include_dir}/*.svh"

echo "Running Kythe verification 'multi file with include dir' test for ${test_name}"
"${VERIBLE_EXTRACTOR_BIN}" --include_dir_paths "${first_include_dir},${second_include_dir}" --file_list_path "${filelist_path}" --file_list_root "${test_dir}" --print_kythe_facts proto > "${test_dir}/entries" ||
    fail "Failed to extract Kythe facts"
echo "Extracted.  Now verifying."
cat "${test_dir}/entries" | "${KYTHE_VERIFIER_BIN}" ${VERILOG_INCLUDE_DIR_TEST_FILES} ||
  fail "Verification failed for ${test_name}"
