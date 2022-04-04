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

# Tests verible-verilog-diff --mode=format

declare -r MY_INPUT_FILE1="${TEST_TMPDIR}/myinput1.txt"
declare -r MY_INPUT_FILE2="${TEST_TMPDIR}/does_not_exist.txt"

# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-diff path."
  exit 1
}
difftool="$(rlocation ${TEST_WORKSPACE}/${1})"

cat >${MY_INPUT_FILE1} <<EOF
  module    m   ;endmodule
EOF

# Run verible-verilog-diff, expecting various error conditions.
# Wrong number of arguments.
"${difftool}" --mode=format "${MY_INPUT_FILE1}"
[[ $? -eq 2 ]] || exit 1

# Nonexistent file (second)
"${difftool}" --mode=format "${MY_INPUT_FILE1}" "${MY_INPUT_FILE2}"
[[ $? -eq 2 ]] || exit 2

# Nonexistent file (first)
"${difftool}" --mode=format "${MY_INPUT_FILE2}" "${MY_INPUT_FILE1}"
[[ $? -eq 2 ]] || exit 3

echo "PASS"
