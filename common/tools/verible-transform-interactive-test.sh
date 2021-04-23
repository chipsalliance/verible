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

# Find input files
MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
readonly MY_INPUT_FILE
MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
readonly MY_OUTPUT_FILE
MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"
readonly MY_EXPECT_FILE

# Process script flags and arguments.
[[ "$#" == 2 ]] || {
  echo "Expecting 2 positional arguments:"
  echo "  verible-transform-interactive.sh"
  echo "  verible-patch-tool"
  exit 1
}

# On Windows, runfiles are not symlinked (unless --enable_runfiles is active,
# which causes other issues on bazel 3.7.0), so use rlocation to find the path
# to the executable. On Unix, you could simply use $1/$2, but using rlocation
# works too
transform_interactive="$(rlocation ${TEST_WORKSPACE}/${1})"
patch_tool="$(rlocation ${TEST_WORKSPACE}/${2})"

transform_command=("$transform_interactive" --patch-tool="$patch_tool")

# expect these two input files to be modified in place (potentially)
cat > "$MY_INPUT_FILE".test1.txt <<EOF
aaa
 bbb
  ccc
 ddd
eee
EOF

cat > "$MY_INPUT_FILE".test2.txt <<EOF
  eee
 ddd
ccc
 bbb
  aaa
EOF

# accept edit on test1 file, but not the test2 file
cat > "$MY_INPUT_FILE".user-input <<EOF
y
n
EOF

# expected results
cat > "$MY_EXPECT_FILE".test1.txt <<EOF
aaa
 bbb
 ddd
eee
EOF

cat > "$MY_EXPECT_FILE".test2.txt <<EOF
  eee
 ddd
ccc
 bbb
  aaa
EOF

# Run 'interactive' patcher.
"${transform_command[@]}" -- grep -v ccc -- "$MY_INPUT_FILE".test*.txt < "$MY_INPUT_FILE".user-input

diff -u --strip-trailing-cr "$MY_EXPECT_FILE".test1.txt "$MY_INPUT_FILE".test1.txt || {
  echo "Expected these files to match, but got the above diff."
  exit 1
}

diff -u --strip-trailing-cr "$MY_EXPECT_FILE".test2.txt "$MY_INPUT_FILE".test2.txt || {
  echo "Expected these files to match, but got the above diff."
  exit 1
}

echo "PASS"
