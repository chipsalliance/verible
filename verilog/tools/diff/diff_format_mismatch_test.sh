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

# Tests verible-verilog-diff --mode=format

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

declare -r MY_INPUT_FILE1="${TEST_TMPDIR}/myinput1.txt"
declare -r MY_INPUT_FILE2="${TEST_TMPDIR}/myinput2.txt"

# Get tool from argument
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-diff path."
  exit 1
}
# On Windows, runfiles are not symlinked (unless --enable_runfiles is active,
# which causes other issues on bazel 3.7.0), so use rlocation to find the path
# to the executable. On Unix, you could simply use $1, but using rlocation
# works too
difftool="$(rlocation ${TEST_WORKSPACE}/${1})"

cat >${MY_INPUT_FILE1} <<EOF
  module    m   ;endmodule
EOF

cat >${MY_INPUT_FILE2} <<EOF
 module
  z

;
endmodule
EOF

# Run verible-verilog-diff.
"${difftool}" --mode=format "${MY_INPUT_FILE1}" "${MY_INPUT_FILE2}"
[[ $? -eq 1 ]] || exit 1

# swap file order
"${difftool}" --mode=format "${MY_INPUT_FILE2}" "${MY_INPUT_FILE1}"
[[ $? -eq 1 ]] || exit 2

echo "PASS"
