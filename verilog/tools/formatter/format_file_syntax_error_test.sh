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

# Tests verible-verilog-format reading from a file, printing to stdout.
# File contains a syntax error.

declare -r MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
declare -r MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"

# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-format path."
  exit 1
}
formatter="$(rlocation ${TEST_WORKSPACE}/${1})"

# syntax error: missing ';' or ports after 'm'
cat >${MY_INPUT_FILE} <<EOF
  module    m   endmodule
EOF

# Run formatter.  Expect error, but that redirected output matches input.
"${formatter}" --nofailsafe_success "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
[[ "$?" -eq 1 ]] || exit 1
diff --strip-trailing-cr "${MY_OUTPUT_FILE}" "${MY_INPUT_FILE}" || exit 2

# Run formatter.  Want 0 exit status.
"${formatter}" --failsafe_success "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
[[ "$?" -eq 0 ]] || exit 3
diff --strip-trailing-cr "${MY_OUTPUT_FILE}" "${MY_INPUT_FILE}" || exit 4

# Default is failsafe_sucess=true.
"${formatter}" "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
[[ "$?" -eq 0 ]] || exit 5
diff --strip-trailing-cr "${MY_OUTPUT_FILE}" "${MY_INPUT_FILE}" || exit 6

echo "PASS"
