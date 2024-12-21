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

MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
readonly MY_OUTPUT_FILE

PATCH_SET=${PATCH:-no}
if [ "${PATCH_SET}" == "no" ]; then
  PATCH=patch
else
  # allow to set patch from environent variable
  PATCH="$(readlink -f $PATCH)"
fi
readonly PATCH

# Process script flags and arguments.
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-lint path."
  exit 1
}
lint_tool="$(rlocation ${TEST_WORKSPACE}/$1)"

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
${TEST_TMPDIR}/syntax-error.sv:2:1-8: syntax error at token "endclass"
endclass
^
EOF

"$lint_tool" "$TEST_FILE" --show_diagnostic_context > "${MY_OUTPUT_FILE}.out"

diff --strip-trailing-cr "${DIAGNOSTIC_OUTPUT}" "${MY_OUTPUT_FILE}.out"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

cat > "${DIAGNOSTIC_OUTPUT}" <<EOF
${TEST_TMPDIR}/syntax-error.sv:2:1-8: syntax error at token "endclass"
EOF

"$lint_tool" "$TEST_FILE" > "${MY_OUTPUT_FILE}.out"

diff --strip-trailing-cr "${DIAGNOSTIC_OUTPUT}" "${MY_OUTPUT_FILE}.out"
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
echo "=== Test module filename rule for stdin"

TEST_FILE="${TEST_TMPDIR}/module-filename-error.sv"

cat > ${TEST_FILE} <<EOF
module m;
endmodule
EOF

"$lint_tool" "$TEST_FILE" --nocheck_syntax > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

cat "$TEST_FILE" | "$lint_tool" - --nocheck_syntax > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

echo "=== Test package filename rule for stdin"
TEST_FILE="${TEST_TMPDIR}/package-filename-error.sv"

cat > ${TEST_FILE} <<EOF
package k;
endpackage
EOF

"$lint_tool" "$TEST_FILE" --nocheck_syntax > /dev/null 2> "${MY_OUTPUT_FILE}.err"

status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

cat "$TEST_FILE" | "$lint_tool" - --nocheck_syntax > /dev/null 2> "${MY_OUTPUT_FILE}.err"

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

WAIVER_FILE="${TEST_TMPDIR}/waive_module-parameter.vlt"
echo "=== Test --waiver_files $WAIVER_FILE with one rule"
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
echo "=== Test --waiver_files $WAIVER_FILE with one rule on one line"
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
echo "=== Test --autofix=inplace"

ORIGINAL_TEST_FILE="${TEST_TMPDIR}/autofix_original.sv"
REFERENCE_TEST_FILE="${TEST_TMPDIR}/autofix_reference.sv"
TEST_FILE="${TEST_TMPDIR}/autofix.sv"

# Fixable violations, in order:
# :2:10: no-trailing-spaces
# :3:10: forbid-consecutive-null-statements
# :4:10: forbid-consecutive-null-statements
# :4:11: no-trailing-spaces
# :5:10: forbid-consecutive-null-statements
# :6:10: forbid-consecutive-null-statements
# :7:10: no-trailing-spaces
# :7:14: posix-eof
>"${ORIGINAL_TEST_FILE}"
echo -en 'module Autofix;    \n' >>"${ORIGINAL_TEST_FILE}"
echo -en '  wire a;;\n'          >>"${ORIGINAL_TEST_FILE}"
echo -en '  wire b;;  \n'        >>"${ORIGINAL_TEST_FILE}"
echo -en '  wire c;;\n'          >>"${ORIGINAL_TEST_FILE}"
echo -en '  wire d;;\n'          >>"${ORIGINAL_TEST_FILE}"
echo -en 'endmodule    '         >>"${ORIGINAL_TEST_FILE}"
>"${REFERENCE_TEST_FILE}"
echo -en 'module Autofix;\n' >>"${REFERENCE_TEST_FILE}"
echo -en '  wire a;\n'       >>"${REFERENCE_TEST_FILE}"
echo -en '  wire b;\n'       >>"${REFERENCE_TEST_FILE}"
echo -en '  wire c;\n'       >>"${REFERENCE_TEST_FILE}"
echo -en '  wire d;\n'       >>"${REFERENCE_TEST_FILE}"
echo -en 'endmodule\n'       >>"${REFERENCE_TEST_FILE}"

cp "${ORIGINAL_TEST_FILE}" "${TEST_FILE}"

RULES_CONFIG_FILE="${TEST_TMPDIR}/rules.conf"
cat > ${RULES_CONFIG_FILE} <<EOF
forbid-consecutive-null-statements
no-trailing-spaces
posix-eof
EOF

"$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" --autofix=inplace \
    "${TEST_FILE}" > /dev/null 2>&1

visualize_invisible_characters() {
  local -r TAB=$'\t'
  local -r CR=$'\r'
  local -r LF=$'\n'
  sed \
      -e ':join' \
      -e 'N' \
      -e '$!b join' \
      -e 's/ /·/g' \
      -e "s/${TAB}/├───/g" \
      -e "s/${CR}/⁋/g" \
      -e "s/\\n/¶\\${LF}/g"
}

check_diff() {
  local reference_file_="$1"
  local test_file_="$2"
  local diff_file_="$3"
  local err_message_="$4"

  local vis_reference_file_="${reference_file_}.vis"
  local vis_test_file_="${test_file_}.vis"

  # Show invisible characters, return error when visualization failed
  visualize_invisible_characters < "$reference_file_" > "$vis_reference_file_" || return 1
  visualize_invisible_characters < "$test_file_"      > "$vis_test_file_"      || return 1

  diff -u "${vis_reference_file_}" "${vis_test_file_}" >"${diff_file_}"
  status="$?"
  (( $status )) && {
    echo "${err_message_}:"
    cat "${diff_file_}"
  }
  return $status
}

DIFF_FILE="${TEST_TMPDIR}/autofix.diff"

FILES_DIFFER_ERR_MESSAGE="Reference file and fixed test file differ"

check_diff "${REFERENCE_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
    "${FILES_DIFFER_ERR_MESSAGE}"|| exit 1

################################################################################
echo "=== Test --autofix=inplace (multiple source files)"

# using same ${RULES_CONFIG_FILE}, ${ORIGINAL_TEST_FILE}, ${REFERENCE_TEST_FILE}

ORIGINAL_TEST_FILE_2="${TEST_TMPDIR}/autofix_2_original.sv"
ORIGINAL_TEST_FILE_3="${TEST_TMPDIR}/autofix_3_original.sv"

REFERENCE_TEST_FILE_2="${TEST_TMPDIR}/autofix_2_reference.sv"
REFERENCE_TEST_FILE_3="${TEST_TMPDIR}/autofix_3_reference.sv"

TEST_FILE_2="${TEST_TMPDIR}/autofix_2.sv"
TEST_FILE_3="${TEST_TMPDIR}/autofix_3.sv"

# No violations
>"${ORIGINAL_TEST_FILE_2}"
echo -en 'module AutofixTwo;\n' >>"${ORIGINAL_TEST_FILE_2}"
echo -en 'endmodule\n'          >>"${ORIGINAL_TEST_FILE_2}"
cp "${ORIGINAL_TEST_FILE_2}" "${REFERENCE_TEST_FILE_2}"

# Fixable violations:
# :1:21: forbid-consecutive-null-statements
# :2:10: no-trailing-spaces
>"${ORIGINAL_TEST_FILE_3}"
echo -en 'module AutofixThree;;\n' >>"${ORIGINAL_TEST_FILE_3}"
echo -en '  wire a;   \n'          >>"${ORIGINAL_TEST_FILE_3}"
echo -en 'endmodule\n'             >>"${ORIGINAL_TEST_FILE_3}"
>"${REFERENCE_TEST_FILE_3}"
echo -en 'module AutofixThree;\n' >>"${REFERENCE_TEST_FILE_3}"
echo -en '  wire a;\n'            >>"${REFERENCE_TEST_FILE_3}"
echo -en 'endmodule\n'            >>"${REFERENCE_TEST_FILE_3}"

cp "${ORIGINAL_TEST_FILE}"   "${TEST_FILE}"
cp "${ORIGINAL_TEST_FILE_2}" "${TEST_FILE_2}"
cp "${ORIGINAL_TEST_FILE_3}" "${TEST_FILE_3}"

"$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" --autofix=inplace \
    "${TEST_FILE}" "${TEST_FILE_2}" "${TEST_FILE_3}" > /dev/null 2>&1

DIFF_FILE_2="${TEST_TMPDIR}/autofix_2.diff"
DIFF_FILE_3="${TEST_TMPDIR}/autofix_3.diff"

failure=0

check_diff "${REFERENCE_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
    "${FILES_DIFFER_ERR_MESSAGE}"
(( failure|="$?" ))

check_diff "${REFERENCE_TEST_FILE_2}" "${TEST_FILE_2}" "${DIFF_FILE_2}" \
    "${FILES_DIFFER_ERR_MESSAGE}"
(( failure|="$?" ))

check_diff "${REFERENCE_TEST_FILE_3}" "${TEST_FILE_3}" "${DIFF_FILE_3}" \
    "${FILES_DIFFER_ERR_MESSAGE}"
(( failure|="$?" ))

(( $failure )) && exit 1

################################################################################
echo "=== Test --autofix=no --autofix_output_file=somefilename"

# if we've not chosen any autofix, then it doesn't matter if the output file
# is bogus.
"$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" \
             --autofix=no \
             --autofix_output_file="/" "${TEST_FILE}" > /dev/null 2>&1

(( $? )) && exit 1

################################################################################
echo "=== Test --autofix=patch --autofix_output_file=<invalidlocation>"

"$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" \
             --autofix=patch \
             --autofix_output_file="/" "${TEST_FILE}" > /dev/null 2>&1

if [ $? -ne 3 ] ; then
  echo "Expected unwritable output patch file to result in exit-code 3"
  exit 1
fi

################################################################################
echo "=== Test --autofix=patch --autofix_output_file=..."

# using same ${RULES_CONFIG_FILE}, ${ORIGINAL_TEST_FILE}, ${REFERENCE_TEST_FILE}

PATCH_FILE="${TEST_TMPDIR}/autofix.patch"
REDIRECT_PATCH_FILE="${TEST_TMPDIR}/redirect-autofix.patch"

cp "${ORIGINAL_TEST_FILE}" "${TEST_FILE}"

"$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" \
             --autofix=patch \
             --autofix_output_file="${PATCH_FILE}" \
             "${TEST_FILE}" > /dev/null 2>&1

# if filename is not given directly we expect output to stdout
"$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" \
             --autofix=patch \
             "${TEST_FILE}" > ${REDIRECT_PATCH_FILE} 2>/dev/null

check_diff "${PATCH_FILE}" "${REDIRECT_PATCH_FILE}" "${DIFF_FILE}" \
           "Patch difference on --autofix_output_file and stdout" || exit 1

NO_CHANGES_ERR_MESSAGE="Expected no changes in input file. Actual changes"

# Test that source file was not modified

check_diff "${ORIGINAL_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
    "${NO_CHANGES_ERR_MESSAGE}" || exit 1

# Patch source file with generated patch file

patch_out="$("${PATCH}" "${TEST_FILE}" "${PATCH_FILE}" 2>&1)"
status="$?"
(( $status )) && {
  echo "Expected exit code 0 from 'patch' tool, but got $status"
  echo "--- 'patch' output [#1] ---"
  echo "$patch_out"
  echo "Patch was"
  echo "------------------"
  cat "${PATCH_FILE}"
  echo "------------------"
  exit 1
}

# Check patched source

check_diff "${REFERENCE_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
    "${FILES_DIFFER_ERR_MESSAGE}" || exit 1

################################################################################
echo "=== Test --autofix=patch --autofix_output_file=... (multiple source files)"

# using same ${ORIGINAL_TEST_FILE}, ${REFERENCE_TEST_FILE},
#            ${ORIGINAL_TEST_FILE_2}, ${REFERENCE_TEST_FILE_2},
#            ${ORIGINAL_TEST_FILE_3}, ${REFERENCE_TEST_FILE_3},
#            ${RULES_CONFIG_FILE},

cp "${ORIGINAL_TEST_FILE}"   "${TEST_FILE}"
cp "${ORIGINAL_TEST_FILE_2}" "${TEST_FILE_2}"
cp "${ORIGINAL_TEST_FILE_3}" "${TEST_FILE_3}"

# `patch` doesn't like absolute paths
REL_TEST_FILE="$(basename ${TEST_FILE})"
REL_TEST_FILE_2="$(basename ${TEST_FILE_2})"
REL_TEST_FILE_3="$(basename ${TEST_FILE_3})"

(
  cd $TEST_TMPDIR
  "$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" --autofix=patch \
      --autofix_output_file="${PATCH_FILE}" \
      "${REL_TEST_FILE}" "${REL_TEST_FILE_2}" "${REL_TEST_FILE_3}" > /dev/null 2>&1
)

failure=0

# Test that source files were not modified

check_diff "${ORIGINAL_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
    "${NO_CHANGES_ERR_MESSAGE}"
(( failure|="$?" ))

check_diff "${ORIGINAL_TEST_FILE_2}" "${TEST_FILE_2}" "${DIFF_FILE_2}" \
    "${NO_CHANGES_ERR_MESSAGE}"
(( failure|="$?" ))

check_diff "${ORIGINAL_TEST_FILE_3}" "${TEST_FILE_3}" "${DIFF_FILE_3}" \
    "${NO_CHANGES_ERR_MESSAGE}"
(( failure|="$?" ))

(( $failure )) && exit 1

# Patch sources with generated patch file

patch_out="$(cd $TEST_TMPDIR; "${PATCH}" -p1 < "${PATCH_FILE}" 2>&1)"
status="$?"
(( $status )) && {
  echo "Expected exit code 0 from 'patch' tool, but got $status"
  echo "--- 'patch' output [#2] ---"
  echo "$patch_out"
  echo "Patch was"
  echo "------------------"
  cat "${PATCH_FILE}"
  echo "------------------"
  exit 1
}

# Check patched sources

check_diff "${REFERENCE_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
    "${FILES_DIFFER_ERR_MESSAGE}"
(( failure|="$?" ))

check_diff "${REFERENCE_TEST_FILE_2}" "${TEST_FILE_2}" "${DIFF_FILE_2}" \
    "${FILES_DIFFER_ERR_MESSAGE}"
(( failure|="$?" ))

check_diff "${REFERENCE_TEST_FILE_3}" "${TEST_FILE_3}" "${DIFF_FILE_3}" \
    "${FILES_DIFFER_ERR_MESSAGE}"
(( failure|="$?" ))

(( $failure )) && exit 1

################################################################################
echo "=== Test --autofix=inplace-interactive"
# using same ${ORIGINAL_TEST_FILE}, ${ORIGINAL_TEST_FILE_2},
#            ${ORIGINAL_TEST_FILE_3}, ${RULES_CONFIG_FILE},

# interactive_autofix_test "<input-keys>" \
#     "reference file 1 contents" "more content"... \
#     --- \
#     "reference file 2 contents" "more content"... \
#     --- \
#     "reference file 3 contents" "more content"...
interactive_autofix_test() {
  local keys_="$1"
  shift
  local -a expected_fixes_=("")
  local source_idx_=0

  while (( $# > 0 )); do
    if [[ "$1" == '---' ]]; then
      (( source_idx_++ ))
      expected_fixes_[source_idx_]=""
    else
      expected_fixes_[source_idx_]+="$1"
    fi
    shift
  done

  cp "${ORIGINAL_TEST_FILE}"   "${TEST_FILE}"
  cp "${ORIGINAL_TEST_FILE_2}" "${TEST_FILE_2}"
  cp "${ORIGINAL_TEST_FILE_3}" "${TEST_FILE_3}"

  # echo -------------
  # echo -en "${expected_fixes_[0]:-}"
  # echo -------------
  # echo -en "${expected_fixes_[1]:-}"
  # echo -------------
  # echo -en "${expected_fixes_[2]:-}"
  # echo -------------

  echo -en "${expected_fixes_[0]:-}" > "${REFERENCE_TEST_FILE}"
  echo -en "${expected_fixes_[1]:-}" > "${REFERENCE_TEST_FILE_2}"
  echo -en "${expected_fixes_[2]:-}" > "${REFERENCE_TEST_FILE_3}"

  # Checking two ways to interact: with patch-interactive and
  # with inplace-interactive

  # In patch interactive mode, we record the patch, check that original
  # files are not modified, and that the output is as expected after
  # application of the patch.

  # `patch` doesn't like absolute paths
  REL_TEST_FILE="$(basename ${TEST_FILE})"
  REL_TEST_FILE_2="$(basename ${TEST_FILE_2})"
  REL_TEST_FILE_3="$(basename ${TEST_FILE_3})"
  ( cd $TEST_TMPDIR
    "$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" \
                 --autofix=patch-interactive --autofix_output_file="${PATCH_FILE}" \
                 "${REL_TEST_FILE}" "${REL_TEST_FILE_2}" "${REL_TEST_FILE_3}" \
                 > /dev/null 2>&1 \
                 <<< "$keys_"
    )

  # Making sure that the original files were not touched
  check_diff "${ORIGINAL_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
    "${NO_CHANGES_ERR_MESSAGE}"
  (( failure|="$?" ))
  check_diff "${ORIGINAL_TEST_FILE_2}" "${TEST_FILE_2}" "${DIFF_FILE}" \
    "${NO_CHANGES_ERR_MESSAGE}"
  (( failure|="$?" ))
  check_diff "${ORIGINAL_TEST_FILE_3}" "${TEST_FILE_3}" "${DIFF_FILE}" \
    "${NO_CHANGES_ERR_MESSAGE}"
  (( failure|="$?" ))

  patch_out="$(cd $TEST_TMPDIR; "${PATCH}" -p1 < "${PATCH_FILE}" 2>&1)"
  status="$?"
  (( $status )) && {
    echo "Expected exit code 0 from 'patch' tool, but got $status"
    echo "--- 'patch' output (interactive_autofix_test) ---"
    echo "$patch_out"
    echo "Patch was"
    echo "------------------"
    cat "${PATCH_FILE}"
    echo "------------------"
    exit 1
  }

  # Check patched sources
  check_diff "${REFERENCE_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
             "Patching - ${FILES_DIFFER_ERR_MESSAGE}"
  (( failure|="$?" ))

  check_diff "${REFERENCE_TEST_FILE_2}" "${TEST_FILE_2}" "${DIFF_FILE_2}" \
             "Patching - ${FILES_DIFFER_ERR_MESSAGE}"
  (( failure|="$?" ))

  check_diff "${REFERENCE_TEST_FILE_3}" "${TEST_FILE_3}" "${DIFF_FILE_3}" \
             "Patching - ${FILES_DIFFER_ERR_MESSAGE}"
  (( failure|="$?" ))

  # Testing interactive modification of the file directly
  cp "${ORIGINAL_TEST_FILE}"   "${TEST_FILE}"
  cp "${ORIGINAL_TEST_FILE_2}" "${TEST_FILE_2}"
  cp "${ORIGINAL_TEST_FILE_3}" "${TEST_FILE_3}"

  "$lint_tool" --ruleset=none --rules_config="${RULES_CONFIG_FILE}" \
      --autofix=inplace-interactive \
      "${TEST_FILE}" "${TEST_FILE_2}" "${TEST_FILE_3}" \
      > /dev/null 2>&1 \
      <<< "$keys_"

  failure=0

  check_diff "${REFERENCE_TEST_FILE}" "${TEST_FILE}" "${DIFF_FILE}" \
             "Inplace - ${FILES_DIFFER_ERR_MESSAGE}"
  (( failure|="$?" ))

  check_diff "${REFERENCE_TEST_FILE_2}" "${TEST_FILE_2}" "${DIFF_FILE_2}" \
             "Inplace - ${FILES_DIFFER_ERR_MESSAGE}"
  (( failure|="$?" ))

  check_diff "${REFERENCE_TEST_FILE_3}" "${TEST_FILE_3}" "${DIFF_FILE_3}" \
             "Inplace - ${FILES_DIFFER_ERR_MESSAGE}"
  (( failure|="$?" ))

  (( $failure )) && exit 1
}

echo "=== Test --autofix=inplace-interactive: Apply All"

interactive_autofix_test "A" \
    'module Autofix;\n' \
    '  wire a;\n'       \
    '  wire b;\n'       \
    '  wire c;\n'       \
    '  wire d;\n'       \
    'endmodule\n'       \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;\n' \
    '  wire a;\n'            \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Reject All"

interactive_autofix_test "D" \
    'module Autofix;    \n' \
    '  wire a;;\n'          \
    '  wire b;;  \n'        \
    '  wire c;;\n'          \
    '  wire d;;\n'          \
    'endmodule    '         \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;;\n' \
    '  wire a;   \n'          \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Reject"

interactive_autofix_test "nA" \
    'module Autofix;    \n' \
    '  wire a;\n'           \
    '  wire b;\n'           \
    '  wire c;\n'           \
    '  wire d;\n'           \
    'endmodule\n'           \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;\n' \
    '  wire a;\n'            \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Apply"

interactive_autofix_test "yD" \
    'module Autofix;\n'    \
    '  wire a;;\n'         \
    '  wire b;;  \n'       \
    '  wire c;;\n'         \
    '  wire d;;\n'         \
    'endmodule    '        \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;;\n' \
    '  wire a;   \n'          \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Apply All For Rule"

interactive_autofix_test "annnnnn" \
    'module Autofix;\n'  \
    '  wire a;;\n'       \
    '  wire b;;\n'       \
    '  wire c;;\n'       \
    '  wire d;;\n'       \
    'endmodule'          \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;;\n' \
    '  wire a;\n'             \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Reject All For Rule"

interactive_autofix_test "dyyyyyy" \
    'module Autofix;    \n' \
    '  wire a;\n'           \
    '  wire b;  \n'         \
    '  wire c;\n'           \
    '  wire d;\n'           \
    'endmodule    \n'       \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;\n' \
    '  wire a;   \n'         \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Reject All For Rule, Apply All For Rule"

interactive_autofix_test "dan" \
    'module Autofix;    \n' \
    '  wire a;\n'           \
    '  wire b;  \n'         \
    '  wire c;\n'           \
    '  wire d;\n'           \
    'endmodule    '         \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;\n' \
    '  wire a;   \n'         \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Show Fix"

interactive_autofix_test "yppnnpypyyppynpypn" \
    'module Autofix;\n'  \
    '  wire a;;\n'       \
    '  wire b;;\n'       \
    '  wire c;\n'        \
    '  wire d;\n'        \
    'endmodule'          \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;\n' \
    '  wire a;   \n'         \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Show Applied Fixes"

interactive_autofix_test "yPPnnPyPyyPPynPyPn" \
    'module Autofix;\n'  \
    '  wire a;;\n' \
    '  wire b;;\n' \
    '  wire c;\n'  \
    '  wire d;\n'  \
    'endmodule'    \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;\n' \
    '  wire a;   \n'         \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: EOF too early"

interactive_autofix_test "y" \
    'module Autofix;\n'    \
    '  wire a;;\n'         \
    '  wire b;;  \n'       \
    '  wire c;;\n'         \
    '  wire d;;\n'         \
    'endmodule    '        \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;;\n' \
    '  wire a;   \n'          \
    'endmodule\n'

echo "=== Test --autofix=inplace-interactive: Unknown keys"

# Only "y" and "D" are valid
interactive_autofix_test "@^(y***D" \
    'module Autofix;\n'    \
    '  wire a;;\n'         \
    '  wire b;;  \n'       \
    '  wire c;;\n'         \
    '  wire d;;\n'         \
    'endmodule    '        \
    --- \
    'module AutofixTwo;\n' \
    'endmodule\n'          \
    --- \
    'module AutofixThree;;\n' \
    '  wire a;   \n'          \
    'endmodule\n'

################################################################################

echo "=== Test --autofix=generate-waiver --autofix_output_file: run linter on a file, then use the generated waiver file to waive all violations"

# File with SV base
TEST_FILE="${TEST_TMPDIR}/generate-waiver.sv"
cat >"${TEST_FILE}" <<EOF
module m;
wire x = y;      
always @* x=y;
always @* x=y;
endmodule


module m;

begin 
    wire x=y; 
end
begin 
   wire s=y; 
end
endmodule


module m;
wire x = y;      
always @* x=y;
always @* x=y;
endmodule
EOF


"$lint_tool" "${TEST_FILE}" --autofix=generate-waiver --autofix_output_file "${MY_OUTPUT_FILE}"  &> /dev/null 
status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

"$lint_tool" "${TEST_FILE}" --waiver_files "${MY_OUTPUT_FILE}"  &>  "${MY_OUTPUT_FILE}.err"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

[[ -s "{$MY_OUTPUT_FILE}.err" ]] && {
  echo "Expected ${MY_OUTPUT_FILE}.err to be empty, but got:"
  cat "${MY_OUTPUT_FILE}.err"
  exit 1
}


################################################################################

echo "=== Test --autofix=inplace-interactive: Choose alternative fix"

# Choose alternatives with key '1' or '2'

# Files with alternatives in autofixes
ORIGINAL_ALT_AUTO_FIX="${TEST_TMPDIR}/orig-autofix-alternative.sv"
cat >"${ORIGINAL_ALT_AUTO_FIX}" <<EOF
module AlternativeAutoFix;
  assign a = 32'h1;
endmodule
EOF

REFERENCE_ALT_AUTO_FIX_1="${TEST_TMPDIR}/alt-autofix-alternative-1.sv"
cat >"${REFERENCE_ALT_AUTO_FIX_1}" <<EOF
module AlternativeAutoFix;
  assign a = 32'h00000001;
endmodule
EOF

REFERENCE_ALT_AUTO_FIX_2="${TEST_TMPDIR}/alt-autofix-alternative-2.sv"
cat >"${REFERENCE_ALT_AUTO_FIX_2}" <<EOF
module AlternativeAutoFix;
  assign a = 32'd1;
endmodule
EOF

failure=0
cp ${ORIGINAL_ALT_AUTO_FIX} ${TEST_FILE}
"$lint_tool" --ruleset=none --rules="undersized-binary-literal=hex:true" \
             --autofix=inplace-interactive \
             "${TEST_FILE}" > /dev/null 2>&1 \
             <<< "1"

check_diff "${REFERENCE_ALT_AUTO_FIX_1}" "${TEST_FILE}" "${DIFF_FILE}" \
    "First alternative not chosen."
(( failure|="$?" ))

# Choosing first non-existing alternative, then an existing one.
cp ${ORIGINAL_ALT_AUTO_FIX} ${TEST_FILE}
"$lint_tool" --ruleset=none --rules="undersized-binary-literal=hex:true" \
             --autofix=inplace-interactive \
             "${TEST_FILE}" > /dev/null 2>&1 \
             <<< "41"

check_diff "${REFERENCE_ALT_AUTO_FIX_1}" "${TEST_FILE}" "${DIFF_FILE}" \
    "First alternative not chosen."
(( failure|="$?" ))

cp ${ORIGINAL_ALT_AUTO_FIX} ${TEST_FILE}
"$lint_tool" --ruleset=none --rules="undersized-binary-literal=hex:true" \
             --autofix=inplace-interactive \
             "${TEST_FILE}" > /dev/null 2>&1 \
             <<< "2"
check_diff "${REFERENCE_ALT_AUTO_FIX_2}" "${TEST_FILE}" "${DIFF_FILE}" \
    "Second alternative not chosen."
(( failure|="$?" ))

(( $failure )) && exit 1

echo "PASS"
