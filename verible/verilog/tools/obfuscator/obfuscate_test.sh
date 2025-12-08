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

# Tests verible-verilog-obfuscate

declare -r MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
declare -r MY_INPUT_FILE2="${TEST_TMPDIR}/myinput2.txt"
declare -r MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
declare -r MY_OUTPUT_FILE2="${TEST_TMPDIR}/myoutput2.txt"
declare -r MY_SAVEMAP_FILE="${TEST_TMPDIR}/save.map"
declare -r MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"

obfuscator="$(rlocation ${TEST_WORKSPACE}/$1)"
difftool="$(rlocation ${TEST_WORKSPACE}/$2)"

###############################################################################
echo "### Simple obfuscation test."

cat >"${MY_INPUT_FILE}" <<EOF
  module    kjasdfASKJdsa18k   ;endmodule
EOF

echo "Run obfuscator."
"${obfuscator}" < "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

echo "Verify output equivalence."
"${difftool}" --mode=obfuscate "${MY_INPUT_FILE}" "${MY_OUTPUT_FILE}"

status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

###############################################################################
echo "### Input lexical error test."

cat >${MY_INPUT_FILE} <<EOF
  module    1kjasdfASKJdsa18k   ;endmodule
EOF

"${obfuscator}" < "${MY_INPUT_FILE}"
status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

###############################################################################
echo "### Tests loading a pre-existing map."

cat >"${MY_SAVEMAP_FILE}" <<EOF
foo yR3
bar igE
asdas iuer8
baz H8u
EOF

cat >"${MY_INPUT_FILE}" <<EOF
  module foo;
    bar baz(.foo(bar));
    endmodule
EOF

cat >"${MY_EXPECT_FILE}" <<EOF
  module yR3;
    igE H8u(.yR3(igE));
    endmodule
EOF

echo "Run obfuscator.  Load and apply previously used substitutions."
"${obfuscator}" --load_map="${MY_SAVEMAP_FILE}" < "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "${MY_OUTPUT_FILE}" "${MY_EXPECT_FILE}" || exit 1

###############################################################################
echo "### Testing reading malformed map file."

# Contains a malformed line.
cat >"${MY_SAVEMAP_FILE}" <<EOF
foo yR3
bar
EOF

cat >"${MY_INPUT_FILE}" <<EOF
  module foo;
    bar baz(.foo(bar));
    endmodule
EOF

echo "Run obfuscator.  Expect error with --load_map on nonexistent file."
"${obfuscator}" --load_map=does.not.exist.file < "${MY_INPUT_FILE}"
status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

echo "Run obfuscator.  Expect error with --load_map on malformed file."
"${obfuscator}" --load_map="${MY_SAVEMAP_FILE}" < "${MY_INPUT_FILE}"
status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

echo "--decode requires a --load_map file."
"${obfuscator}" --decode < "${MY_INPUT_FILE}"
status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

###############################################################################
echo "### Testing obfuscate with --save_map"

cat >"${MY_INPUT_FILE}" <<EOF
  module
foo_bar
    ;endmodule
EOF

cat >"${MY_INPUT_FILE2}" <<EOF
  foo_bar + foo_bar
EOF

echo "Run obfuscator.  Save substitutions."
"${obfuscator}" --save_map="${MY_SAVEMAP_FILE}" < "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

# The input file has only one line that doesn't match 'module'.
# That line contains the encoded identifier of interest.
# Use that to generate the expected file.
foo_bar_encoded=$(grep -v module "${MY_OUTPUT_FILE}")
cat >"${MY_EXPECT_FILE}" <<EOF
  ${foo_bar_encoded} + ${foo_bar_encoded}
EOF

echo "Re-apply saved substitution on new file."
"${obfuscator}" --load_map="${MY_SAVEMAP_FILE}" < "${MY_INPUT_FILE2}" > "${MY_OUTPUT_FILE2}"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "${MY_OUTPUT_FILE2}" "${MY_EXPECT_FILE}" || exit 1

###############################################################################
echo "Test obfuscate with --save_map to a bad path"

cat >"${MY_INPUT_FILE}" <<EOF
  module foo;
    bar baz(.foo(bar));
    endmodule
EOF

echo "Run obfuscator.  Expect error with --save_map."
"${obfuscator}" --save_map=does/not/exist/dir/file.map < "${MY_INPUT_FILE}"
status="$?"
[[ $status == 1 ]] || {
  echo "Expected exit code 1, but got $status"
  exit 1
}

###############################################################################
echo "Test obfuscate --decode mode"

cat >"${MY_INPUT_FILE}" <<EOF
  module foo_bar;
    foo bar(.baz(baz),
      .clk(clk));
  endmodule
EOF

echo "Run obfuscator.  Save substitutions."
"${obfuscator}" --save_map="${MY_SAVEMAP_FILE}" < "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

echo "Decode the previous output using saved substitutions."
"${obfuscator}" --decode --load_map="${MY_SAVEMAP_FILE}" < "${MY_OUTPUT_FILE}" > "${MY_OUTPUT_FILE2}"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "${MY_OUTPUT_FILE2}" "${MY_INPUT_FILE}" || exit 1

##############################################################################
echo "Test built-in functions untouched"
cat >"${MY_INPUT_FILE}" <<EOF
parameter a = sin(1); parameter b = ceil(2.5);
EOF

${obfuscator} < "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

cat "${MY_OUTPUT_FILE}"

grep "sin(.*ceil(" "${MY_OUTPUT_FILE}"
status="$?"
[[ $status == 0 ]] || {
  echo "output does not contain preserved built-in function names"
  exit 1
}

###############################################################################
echo "Test obfuscate --preserve_interface mode"

cat >"${MY_INPUT_FILE}" <<EOF
  module foo_bar(input clk);
    foo bar(.baz(baz),
      .clk(clk));
  endmodule
EOF

echo "Run obfuscator.  Save substitutions."
"${obfuscator}" --preserve_interface --save_map="${MY_SAVEMAP_FILE}" < "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
status="$?"
[[ $status == 0 ]] || {
  echo "Expected exit code 0, but got $status"
  exit 1
}

intact_names=(foo_bar clk)
echo "Verify the saved map does not change interface names."
for intact_name in ${intact_names[@]}; do
  grep "^${intact_name} ${intact_name}[[:space:]]*\$" "${MY_SAVEMAP_FILE}" > /dev/null
  status="$?"
  [[ $status == 0 ]] || {
    echo "Failed to check mapping for name: ${intact_name}"
    cat "${MY_SAVEMAP_FILE}"
    exit 1
  }
done

###############################################################################
echo "PASS"
