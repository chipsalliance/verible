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

# Find input files
MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
readonly MY_INPUT_FILE
MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
readonly MY_OUTPUT_FILE
MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"
readonly MY_EXPECT_FILE

# Process script flags and arguments.
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-patch-tool path."
  exit 1
}
patch_tool="$1"

################################################################################
# Test no command.

"$patch_tool" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
# Test invalid command.

"$patch_tool" wish > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
# Test the 'help' command.

"$patch_tool" help > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "changed-lines" "$MY_OUTPUT_FILE" || {
  echo "Expected \"changed-lines\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
# Test 'help' on a specific command.

"$patch_tool" help help > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

################################################################################
# Test 'changed-lines' missing argument.

"$patch_tool" changed-lines > /dev/null

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
# Test the 'changed-lines' command on a fake patch file.

cat > "$MY_INPUT_FILE" <<EOF
--- /path/to/file1.txt	2017-01-01
+++ /path/to/file1.txt	2017-01-01
@@ -9,3 +12,6 @@
 same
+new
+new too
 same
+new three
 same
--- /dev/null	2017-01-01
+++ /path/to/file2.txt	2017-01-01
@@ -0,0 +1,3 @@
+do
+re
+mi
EOF

cat > "$MY_EXPECT_FILE" <<EOF
/path/to/file1.txt 13-14,16
/path/to/file2.txt
EOF

"$patch_tool" changed-lines "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

# Same but piped into stdin.

"$patch_tool" changed-lines - < "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
# Test reading a malformed patch file.

cat > "$MY_INPUT_FILE" <<EOF
--- /path/to/file1.txt	2017-01-01
+++ /path/to/file1.txt	2017-01-01
@@ -1,1 +1,1 @@
EOF

"$patch_tool" changed-lines "$MY_INPUT_FILE" > /dev/null

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
# Test reading a nonexistent patch file.

"$patch_tool" changed-lines "$MY_INPUT_FILE.does.not.exist" > /dev/null

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
# Test stdin-test noninteractively.

cat > "$MY_INPUT_FILE" <<EOF
Lovely file!
EOF

"$patch_tool" stdin-test < "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "Lovely file" "$MY_OUTPUT_FILE" || {
  echo "Expected \"Lovely file\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
# Test cat-test noninteractively.

cat > "$MY_INPUT_FILE".1 <<EOF
111111
EOF

cat > "$MY_INPUT_FILE".2 <<EOF
222222
EOF

"$patch_tool" cat-test "$MY_INPUT_FILE".1 > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "111111" "$MY_OUTPUT_FILE" || {
  echo "Expected \"111111\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

# Test single redirection
"$patch_tool" cat-test - < "$MY_INPUT_FILE".1 > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "111111" "$MY_OUTPUT_FILE" || {
  echo "Expected \"111111\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

# Test single redirection with one other file
"$patch_tool" cat-test - "$MY_INPUT_FILE".2 < "$MY_INPUT_FILE".1 > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "111111" "$MY_OUTPUT_FILE" || {
  echo "Expected \"111111\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

grep -q "222222" "$MY_OUTPUT_FILE" || {
  echo "Expected \"222222\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

# Test single redirection with one other file, different order
"$patch_tool" cat-test "$MY_INPUT_FILE".2 - < "$MY_INPUT_FILE".1 > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "111111" "$MY_OUTPUT_FILE" || {
  echo "Expected \"111111\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

grep -q "222222" "$MY_OUTPUT_FILE" || {
  echo "Expected \"222222\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

rm -f "$MY_INPUT_FILE".{1,2}

################################################################################
echo "PASS"
