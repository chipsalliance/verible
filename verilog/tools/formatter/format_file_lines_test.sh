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

# Tests verible-verilog-format reading from a file, selecting --lines.

# --- begin runfiles.bash initialization v2 ---
# Copy-pasted from the Bazel Bash runfiles library v2.
set -uo pipefail; f=bazel_tools/tools/bash/runfiles/runfiles.bash
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
  source "$0.runfiles/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  { echo>&2 "ERROR: cannot find $f"; exit 1; }; f=; set -e
# --- end runfiles.bash initialization v2 ---

# Remove exit on failure set by runfiles.bash...
set +e

declare -r MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
declare -r MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
declare -r MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"

# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-format path."
  exit 1
}
# On Windows, runfiles are not symlinked (unless --enable_runfiles is active,
# which causes other issues on bazel 3.7.0), so use rlocation to find the path
# to the executable. On Unix, you could simply use $1, but using rlocation
# works too
formatter="$(rlocation ${TEST_WORKSPACE}/${1})"

cat >${MY_INPUT_FILE} <<EOF
   parameter   int  var_line_1  =  1  ;
   parameter   int  var_line_2  =  2  ;
   parameter   int  var_line_3  =  3  ;
   parameter   int  var_line_4  =  4  ;
// verilog_format: off
   parameter   int  var_line_6  =  6  ;
   parameter   int  var_line_7  =  7  ;
// verilog_format: on
   parameter   int  var_line_9  =  9  ;
   parameter   int  var_line_10  =  10  ;
   parameter   int  var_line_11  =  11  ;
   parameter   int  var_line_12  =  12  ;
   parameter   int  var_line_13  =  13  ;
   parameter   int  var_line_14  =  14  ;
   parameter   int  var_line_15  =  15  ;
   parameter   int  var_line_16  =  16  ;
   parameter   int  var_line_17  =  17  ;
EOF

cat >${MY_EXPECT_FILE} <<EOF
   parameter   int  var_line_1  =  1  ;
   parameter   int  var_line_2  =  2  ;
   parameter   int  var_line_3  =  3  ;
parameter int var_line_4 = 4;
// verilog_format: off
   parameter   int  var_line_6  =  6  ;
   parameter   int  var_line_7  =  7  ;
// verilog_format: on
parameter int var_line_9 = 9;
   parameter   int  var_line_10  =  10  ;
   parameter   int  var_line_11  =  11  ;
parameter int var_line_12 = 12;
parameter int var_line_13 = 13;
   parameter   int  var_line_14  =  14  ;
parameter int var_line_15 = 15;
parameter int var_line_16 = 16;
   parameter   int  var_line_17  =  17  ;
EOF

# Run formatter.
${formatter} --lines 4-9 --lines 12-13,15,16 ${MY_INPUT_FILE} \
  > ${MY_OUTPUT_FILE} || exit 1
diff --strip-trailing-cr "${MY_OUTPUT_FILE}" "${MY_EXPECT_FILE}" || exit 2

echo "PASS"
