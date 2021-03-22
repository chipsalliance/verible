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
echo "=== Test no file"

"$lint_tool" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

################################################################################
echo "=== Test the '--helpfull' flag"

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
echo "=== Test the '--help_rules' flag"

"$lint_tool" --help_rules=no-tabs > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

################################################################################
echo "=== Test the '--generate_markdown' flag"

"$lint_tool" --generate_markdown > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

[[ -s "$MY_OUTPUT_FILE" ]] || {
  echo "Expected $MY_OUTPUT_FILE to be non-empty, but got empty."
  exit 1
}

################################################################################
echo "=== Test --parse_fatal (default)"

TEST_FILE="${TEST_TMPDIR}/syntax-error.sv"

cat > ${TEST_FILE} <<EOF
class c  // missing semicolon
endclass
EOF

"$lint_tool" "$TEST_FILE" > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

echo "=== Test --show_diagnostic_context"
"$lint_tool" "$TEST_FILE" --show_diagnostic_context > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

DIAGNOSTIC_OUTPUT="${TEST_TMPDIR}/expected-diagnostic.out"
cat > "${DIAGNOSTIC_OUTPUT}" <<EOF
${TEST_TMPDIR}/syntax-error.sv:2:1: syntax error, rejected "endclass" (syntax-error).
endclass
^
EOF

"$lint_tool" "$TEST_FILE" --show_diagnostic_context > "${MY_OUTPUT_FILE}.out"

diff "${DIAGNOSTIC_OUTPUT}" "${MY_OUTPUT_FILE}.out"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

cat > "${DIAGNOSTIC_OUTPUT}" <<EOF
${TEST_TMPDIR}/syntax-error.sv:2:1: syntax error, rejected "endclass" (syntax-error).
EOF

"$lint_tool" "$TEST_FILE" > "${MY_OUTPUT_FILE}.out"

diff "${DIAGNOSTIC_OUTPUT}" "${MY_OUTPUT_FILE}.out"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

echo "=== Test --parse_fatal"
"$lint_tool" "$TEST_FILE" --parse_fatal > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

echo "=== Test --noparse_fatal"
"$lint_tool" "$TEST_FILE" --noparse_fatal > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

################################################################################
echo "=== Test --check_syntax (default) and --parse_fatal (default)"

TEST_FILE="${TEST_TMPDIR}/syntax-error.sv"

cat > ${TEST_FILE} <<EOF
class c  // missing semicolon
endclass
EOF

"$lint_tool" "$TEST_FILE" > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

echo "=== Test --check_syntax"
"$lint_tool" "$TEST_FILE" --check_syntax > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

echo "=== Test --nocheck_syntax"
"$lint_tool" "$TEST_FILE" --nocheck_syntax > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

[[ ! -s "${MY_OUTPUT_FILE}.err" ]] || {
  echo "Expected ${MY_OUTPUT_FILE}.err to be empty, but got:"
  cat "${MY_OUTPUT_FILE}.err"
  exit 1
}

################################################################################
echo "=== Test --lint_fatal (default)"

TEST_FILE="${TEST_TMPDIR}/lint-error.sv"

cat > "${TEST_FILE}" <<EOF
class	c;  // tabs
endclass
EOF

"$lint_tool" --rules=no-tabs "$TEST_FILE" > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

echo "=== Test --lint_fatal"
"$lint_tool" --rules=no-tabs --lint_fatal "$TEST_FILE" > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

echo "=== Test --nolint_fatal"
"$lint_tool" --rules=no-tabs --nolint_fatal "$TEST_FILE" > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

################################################################################
echo "=== Test invalid rule (--rules)"

# using same ${TEST_FILE}

echo "=== Test --nolint_fatal"
"$lint_tool" --rules=does-not-exist-fake-rule --nolint_fatal "$TEST_FILE" > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Test '--waiver_files'."

TEST_FILE="${TEST_TMPDIR}/instance_parameters.sv"

cat > "${TEST_FILE}" <<EOF
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

echo "=== Test --waiver_files $WAIVER_FILE with one rule"
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

echo "=== Test --waiver_files $WAIVER_FILE with one rule on one line"
WAIVER_FILE="${TEST_TMPDIR}/waive_module-parameter_line.vlt"
cat > ${WAIVER_FILE} <<EOF
waive --rule=module-parameter --line=1
EOF

# This is buggy: the error message actually comes out of stdout instead of stderr. This
# needs to be fixed in a separate change, but for now we merge stdout and stderr.
"$lint_tool" "$TEST_FILE" --waiver_files "$WAIVER_FILE" > "${MY_OUTPUT_FILE}.err" 2>&1

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

grep -q "module-parameter" "${MY_OUTPUT_FILE}.err" || {
  echo "Expected \"module-parameter\" in ${MY_OUTPUT_FILE}.err but didn't find it.  Got:"
  cat "${MY_OUTPUT_FILE}.err"
  exit 1
}

################################################################################
echo "PASS"
