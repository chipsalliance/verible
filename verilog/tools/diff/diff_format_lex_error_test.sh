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
declare -r MY_INPUT_FILE2="${TEST_TMPDIR}/myinput2.txt"

# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-diff path."
  exit 1
}
difftool="$(rlocation ${TEST_WORKSPACE}/${1})"

# contains lexical error token "1mm"
cat >${MY_INPUT_FILE1} <<EOF
  module    1mm   ;endmodule
EOF

cat >${MY_INPUT_FILE2} <<EOF
 module
  m

;
endmodule
EOF

# Run verible-verilog-diff.
"${difftool}" --mode=format "${MY_INPUT_FILE1}" "${MY_INPUT_FILE2}"
[[ $? -eq 1 ]] || exit 1

# switch files
"${difftool}" --mode=format "${MY_INPUT_FILE2}" "${MY_INPUT_FILE1}"
[[ $? -eq 1 ]] || exit 2

echo "PASS"
