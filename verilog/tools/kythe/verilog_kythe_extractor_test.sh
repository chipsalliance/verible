#!/usr/bin/env bash
# Copyright 2017-2020 The Verible Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Find input files
MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
readonly MY_INPUT_FILE
MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
readonly MY_OUTPUT_FILE
MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"
readonly MY_EXPECT_FILE

# Process script flags and arguments.
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-kythe-extractor path."
  exit 1
}
extractor="$(rlocation ${TEST_WORKSPACE}/${1})"

################################################################################
echo "=== Test no arguments."

"$extractor" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Test '--helpfull'."

"$extractor" --helpfull > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

grep -q "json" "$MY_OUTPUT_FILE" || {
  echo "Expected \"json\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Expect failure on nonexistent file list"

 # Construct a file-list on-the-fly as a file-descriptor
"$extractor" \
  --file_list_path "${TEST_TMPDIR}/nonexistent.txt" \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  --print_kythe_facts=json \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Expect failure on nonexistent file listed in the file list"

# Construct a file-list
echo "nonexistent.sv" > "${TEST_TMPDIR}/file_list"
"$extractor" \
  --file_list_path "${TEST_TMPDIR}/file_list" \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  --print_kythe_facts=json \
  > "$MY_OUTPUT_FILE" 2>&1

# Exit status 0 to tolerate missing files.
status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

# Make sure we see some diagnostic message about missing files.
grep -q "Encountered some issues while indexing files" "$MY_OUTPUT_FILE" || {
  echo "Expected \"Encountered some issues while indexing files\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Extract one file, printing encoded JSON for debug."

cat > "$MY_INPUT_FILE" <<EOF
localparam int fooo = 1;
localparam int barr = fooo;
EOF

# Construct a file-list
echo "myinput.txt" > "${TEST_TMPDIR}/file_list"
"$extractor" \
  --file_list_path "${TEST_TMPDIR}/file_list" \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  --print_kythe_facts=json \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "signature" "$MY_OUTPUT_FILE" || {
  echo "Expected \"signature\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Extract one file, printing human-readable JSON for debug."

cat > "$MY_INPUT_FILE" <<EOF
localparam int fooo = 1;
localparam int barr = fooo;
EOF

# Construct a file-list
echo "myinput.txt" > "${TEST_TMPDIR}/file_list"
"$extractor" \
  --file_list_path "${TEST_TMPDIR}/file_list" \
  --file_list_root "$(dirname "$MY_INPUT_FILE")" \
  --print_kythe_facts=json_debug \
  > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "fooo" "$MY_OUTPUT_FILE" || {
  echo "Expected \"fooo\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

grep -q "barr" "$MY_OUTPUT_FILE" || {
  echo "Expected \"fooo\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "PASS"
