#!/bin/bash
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

MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
readonly MY_OUTPUT_FILE

# Process script flags and arguments.
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-lint path."
  exit 1
}
lint_tool="$1"

################################################################################
# Test no command.

"$lint_tool" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

################################################################################
# Test the '--helpfull' command.

"$lint_tool" --helpfull > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

grep -q -e "--rules" "$MY_OUTPUT_FILE" || {
  echo "Expected \"--rules\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
# Test '--waiver_files'.
TEST_FILE="${TEST_TMPDIR}/instance_parameters.sv"

cat > ${TEST_FILE} <<EOF
module instance_parameters;
  bar #(4, 8) baz;
endmodule
EOF

"$lint_tool" "$TEST_FILE" --waiver_files > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

grep -q "ERROR: Missing the value for the flag 'waiver_files'" "${MY_OUTPUT_FILE}.err" || {
  echo "Expected \"ERROR: Missing the value for the flag 'waiver_files'\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "${MY_OUTPUT_FILE}.err"
  exit 1
}

WAIVER_FILE="${TEST_TMPDIR}/waive_module-parameter.vlt"
cat > ${WAIVER_FILE} <<EOF
waive --rule=module-parameter
EOF

"$lint_tool" "$TEST_FILE" --waiver_files "$WAIVER_FILE" > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

WAIVER_FILE="${TEST_TMPDIR}/waive_module-parameter_line.vlt"
cat > ${WAIVER_FILE} <<EOF
waive --rule=module-parameter --line=1
EOF

# This is buggy: the error message actually comes out of stdout instead of stderr. This
# needs to be fixed in a separate change, but for now we merge stdout and stderr.
"$lint_tool" "$TEST_FILE" --waiver_files "$WAIVER_FILE" > "${MY_OUTPUT_FILE}.err" 2>&1

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

grep -q "module-parameter" "${MY_OUTPUT_FILE}.err" || {
  echo "Expected \"module-parameter\" in ${MY_OUTPUT_FILE}.err but didn't find it.  Got:"
  cat "${MY_OUTPUT_FILE}.err"
  exit 1
}

################################################################################
echo "PASS"
