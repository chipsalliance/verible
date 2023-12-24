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

# Tests verible-verilog-format reading from a file, checking for formatting
# changes where changes are needed. This should return 1.


declare -r MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"

# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-format path."
  exit 1
}
formatter="$(rlocation ${TEST_WORKSPACE}/${1})"

cat >${MY_INPUT_FILE} <<EOF
  module    m   ;endmodule
EOF

# Run formatter.
${formatter} --verbose --verify ${MY_INPUT_FILE}
if [ "$?" -eq 0 ]; then
    echo "Changes should produce non-zero error code"
    echo "FAIL"
    exit 1
fi

echo "PASS"
