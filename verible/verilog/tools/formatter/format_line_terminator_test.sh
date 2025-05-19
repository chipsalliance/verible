#!/usr/bin/env bash
# Copyright 2017-2025 The Verible Authors.
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

declare -r MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
declare -r MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
declare -r MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"

# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-format path."
  exit 1
}
formatter="$(rlocation ${TEST_WORKSPACE}/${1})"

# Create files with LF and CRLF line terminators. They are properly formatted, so running
# the formatter should only change line endings
printf "// some comment\n/* some other comment */\nmodule m;\nendmodule\n" > "${MY_INPUT_FILE}LF"
printf "// some comment\r\n/* some other comment */\r\nmodule m;\r\nendmodule\r\n" > "${MY_INPUT_FILE}CRLF"


for newline in LF CRLF; do
  cp "${MY_INPUT_FILE}$newline" "${MY_EXPECT_FILE}$newline"
done

# Test any combination of input line terminators and output line terminators.
# Test both inline formatting and standard output
for original_newline in LF CRLF; do
  for target_newline in LF CRLF; do
    PROPER_INPUT_FILE="${MY_INPUT_FILE}$original_newline"
    PROPER_EXPECT_FILE="${MY_EXPECT_FILE}$target_newline"

    ${formatter} --line_terminator=$target_newline $PROPER_INPUT_FILE > ${MY_OUTPUT_FILE}
    cmp ${MY_OUTPUT_FILE} $PROPER_EXPECT_FILE || exit 1

    cp $PROPER_INPUT_FILE ${MY_OUTPUT_FILE}
    ${formatter} --line_terminator=$target_newline --inplace ${MY_OUTPUT_FILE}
    cmp ${MY_OUTPUT_FILE} $PROPER_EXPECT_FILE || exit 2
  done
done

echo "PASS"
