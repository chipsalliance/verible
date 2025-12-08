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

# Tests the --inplace flag of verible-verilog-format.

declare -r MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
declare -r MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"

# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-format path."
  exit 1
}
formatter="$(rlocation ${TEST_WORKSPACE}/$1)"

# Will overwrite this file in-place.
cat >${MY_OUTPUT_FILE} <<EOF
  module    m   ;endmodule
EOF

cat >${MY_EXPECT_FILE} <<EOF
module m;
endmodule
EOF

# Run formatter.
${formatter} --inplace ${MY_OUTPUT_FILE} || exit 1
diff --strip-trailing-cr "${MY_OUTPUT_FILE}" "${MY_EXPECT_FILE}" || exit 2

echo "PASS"
