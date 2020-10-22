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

# Tests verible-verilog-obfuscate

source googletest.sh || exit 1

export RUNFILES="$TEST_SRCDIR"

declare -r MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
declare -r MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"

diff_package=third_party/verible/verilog/tools/diff
obfuscate_package=third_party/verible/verilog/tools/obfuscator
difftool="${RUNFILES}/google3/$diff_package/verible-verilog-diff"
obfuscator="${RUNFILES}/google3/$obfuscate_package/verible-verilog-obfuscate"

cat >${MY_INPUT_FILE} <<EOF
  module    kjasdfASKJdsa18k   ;endmodule
EOF

# Run obfuscator.
"${obfuscator}" < "${MY_INPUT_FILE}" > "${MY_OUTPUT_FILE}"
check_eq $? 0
# Verify output equivalence.
"${difftool}" --mode=obfuscate "${MY_INPUT_FILE}" "${MY_OUTPUT_FILE}"
check_eq $? 0

echo "PASS"

