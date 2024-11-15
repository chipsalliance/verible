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

# Tests verible-verilog-format reading from single or multiple files, checking
# for formatting changes where changes are needed. This should return 1 if any
# of the files are incorrectly formatted.


# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-format path."
  exit 1
}
formatter="$(rlocation ${TEST_WORKSPACE}/${1})"

# create 1 formatted and 1 unformatted file
unformatted="${TEST_TMPDIR}/unformatted.sv"
formatted="${TEST_TMPDIR}/formatted.sv"
cat >"${unformatted}" <<EOF
  module    m   ;endmodule
EOF
cp "$unformatted" "$formatted"
$formatter --inplace "$formatted"

cases=(
    "$formatted"
    "$unformatted"
    "$unformatted $unformatted"
    "$unformatted $formatted"
    "$formatted $unformatted"
    "$formatted $formatted"
)

for files in "${cases[@]}"; do
    echo "$files" | grep -q unformatted.sv
    expected_ret=$((!$?))
    echo Formatting...
    ${formatter} --verbose --verify --inplace $files
    actual_ret=$?
    if [ "$actual_ret" -ne "$expected_ret" ]; then
        echo "Expected return code $expected_ret, got $actual_ret"
        echo "FAIL"
        exit 1
    fi
done

echo "PASS"
