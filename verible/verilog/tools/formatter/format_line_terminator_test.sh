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

# Will overwrite this file in-place.
cat >${MY_INPUT_FILE} <<EOF
// some comment
/* some other comment */
module m;
endmodule
EOF

cp ${MY_INPUT_FILE} ${MY_EXPECT_FILE}

for original_newline in LF CRLF; do
  if [[ "$original_newline" == "CRLF" ]]; then
    unix2dos ${MY_INPUT_FILE}
  else
    dos2unix ${MY_INPUT_FILE}
  fi

  for target_newline in LF CRLF; do
    if [[ "$target_newline" == "CRLF" ]]; then
      unix2dos ${MY_EXPECT_FILE}
    else
      dos2unix ${MY_EXPECT_FILE}
    fi

    ${formatter} --line_terminator=$target_newline ${MY_INPUT_FILE} > ${MY_OUTPUT_FILE}
    cmp ${MY_OUTPUT_FILE} ${MY_EXPECT_FILE} || exit 1

    cp ${MY_INPUT_FILE} ${MY_OUTPUT_FILE}
    ${formatter} --line_terminator=$target_newline --inplace ${MY_OUTPUT_FILE}
    cmp ${MY_OUTPUT_FILE} ${MY_EXPECT_FILE} || exit 1
  done
done

echo "PASS"
