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

# Find input files
readonly MY_RELATIVE_INCLUDED_FILE_1="myinclude_1.txt"
readonly MY_ABSOLUTE_INCLUDED_FILE_1="${TEST_TMPDIR}/${MY_RELATIVE_INCLUDED_FILE_1}"
readonly MY_INCLUDED_FILE_PATH_1="${MY_ABSOLUTE_INCLUDED_FILE_1%$MY_RELATIVE_INCLUDED_FILE_1}"
readonly MY_RELATIVE_INCLUDED_FILE_2="myinclude_2.txt"
readonly MY_ABSOLUTE_INCLUDED_FILE_2="${TEST_TMPDIR}/${MY_RELATIVE_INCLUDED_FILE_2}"
readonly MY_INCLUDED_FILE_PATH_2="${MY_ABSOLUTE_INCLUDED_FILE_2%$MY_RELATIVE_INCLUDED_FILE_2}"
readonly MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
readonly MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
readonly MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"

# Process script flags and arguments.
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-preprocessor path."
  exit 1
}
preprocessor="$(rlocation ${TEST_WORKSPACE}/$1)"

################################################################################
echo "=== Line:${LINENO} Test no command."

"$preprocessor" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test invalid command."

"$preprocessor" bad-subcommand > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test the 'help' command."

"$preprocessor" help > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

grep -q "strip-comments" "$MY_OUTPUT_FILE" || {
  echo "Expected \"strip-comments\" in $MY_OUTPUT_FILE but didn't find it.  Got:"
  cat "$MY_OUTPUT_FILE"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test 'help' on a specific command."

"$preprocessor" help help > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test strip-comments: missing file argument."

"$preprocessor" strip-comments > /dev/null

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test strip-comments: white out comments"

cat > "$MY_INPUT_FILE" <<EOF
// fake Verilog file.
/*
  file description
*/
module mmm;
  logic l1;  // l1 is for blah
  /* l2 does this */
  logic l2;
endmodule
EOF

cat > "$MY_EXPECT_FILE" <<EOF
                     
  
                  
  
module mmm;
  logic l1;                   
                    
  logic l2;
endmodule
EOF

"$preprocessor" strip-comments "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

# Same but piped into stdin.

"$preprocessor" strip-comments - < "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test strip-comments: delete comments"

cat > "$MY_INPUT_FILE" <<EOF
// fake Verilog file.
/*
  file description
*/
module mmm;
  logic l1;  // l1 is for blah
  /* l2 does this */
  logic l2;
endmodule
EOF

cat > "$MY_EXPECT_FILE" <<EOF

 
module mmm;
  logic l1;  
   
  logic l2;
endmodule
EOF

"$preprocessor" strip-comments "$MY_INPUT_FILE" "" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test strip-comments: replace comments"

cat > "$MY_INPUT_FILE" <<EOF
// fake Verilog file.
/*
  file description
*/
module mmm;
  logic l1;  // l1 is for blah
  /* l2 does this */
  logic l2;
endmodule
EOF

cat > "$MY_EXPECT_FILE" <<EOF
//...................
/*
..................
*/
module mmm;
  logic l1;  //...............
  /*..............*/
  logic l2;
endmodule
EOF

"$preprocessor" strip-comments "$MY_INPUT_FILE" . > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}


################################################################################
echo "=== Line:${LINENO} Test strip-comments: on a lexically invalid source file"

cat > "$MY_INPUT_FILE" <<EOF
module 1m; /* comment */ endmodule
EOF

cat > "$MY_EXPECT_FILE" <<EOF
module 1m;               endmodule
EOF

"$preprocessor" strip-comments "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}


################################################################################
# Test strip-comments: reading a nonexistent source file.

"$preprocessor" strip-comments "$MY_INPUT_FILE.does.not.exist" > /dev/null

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}
################################################################################
echo "=== Line:${LINENO} Test preprocess: on a source file with conditionals"

cat > "$MY_INPUT_FILE" <<EOF
module m();
  wire x = 1;
\`ifdef A
  real is_defined = 1;
\`else
  real is_defined = 0;
\`endif
// pre-blank line

// post-blank line
endmodule
EOF

cat > "$MY_EXPECT_FILE" <<EOF
module m();
  wire x = 1;

  real is_defined = 0;

// pre-blank line

// post-blank line
endmodule
EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test generate-variants: on a source file with nested conditionals"

cat > "$MY_INPUT_FILE" <<EOF
\`ifdef A
  A_TRUE
  \`ifdef B
    B_TRUE
  \`else
    B_FALSE
  \`endif
\`else
  A_FALSE
\`endif

\`ifdef A
  A_TRUE
\`endif

EOF

cat > "$MY_EXPECT_FILE" <<EOF
Variant number 1:
(#293: "A_TRUE")
(#293: "B_TRUE")
(#293: "A_TRUE")
Variant number 2:
(#293: "A_TRUE")
(#293: "B_FALSE")
(#293: "A_TRUE")
Variant number 3:
(#293: "A_FALSE")
EOF

"$preprocessor" generate-variants "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test generate-variants: with limited variants"

cat > "$MY_INPUT_FILE" <<EOF
\`ifdef A
  A_TRUE
\`else
  A_FALSE
\`endif

\`ifdef A
  A_TRUE
\`else
  A_FALSE
\`endif

\`ifndef A
  A_FALSE
\`else
  A_TRUE
\`endif
EOF

cat > "$MY_EXPECT_FILE" <<EOF
Variant number 1:
(#293: "A_TRUE")
(#293: "A_TRUE")
(#293: "A_TRUE")
EOF

"$preprocessor" generate-variants "$MY_INPUT_FILE" --limit_variants 1 > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test generate-variants: with invalid conditionals"

cat > "$MY_INPUT_FILE" <<EOF
\`ifdef A
  A_TRUE
\`else
  A_FALSE
EOF

"$preprocessor" generate-variants "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test generate-variants: with multiple files"

cat > "$MY_INPUT_FILE" <<EOF
\`ifdef A
  A_TRUE
\`else
  A_FALSE
\`endif
EOF

"$preprocessor" generate-variants "$MY_INPUT_FILE" "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: with missing files"

"$preprocessor" preprocess > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test generate-variants: with multiple files"

cat > "$MY_INPUT_FILE" <<EOF
\`ifdef A
  A_TRUE
\`else
  A_FALSE
\`endif
EOF

"$preprocessor" generate-variants "$MY_INPUT_FILE" "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: with multiple files"

cat > "$MY_INPUT_FILE" <<EOF
\`ifdef A
  A_TRUE
\`else
  A_FALSE
\`endif
EOF


cat > "$MY_EXPECT_FILE" <<EOF

  A_FALSE


  A_FALSE

EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: passing defines"

cat > "$MY_INPUT_FILE" <<EOF
\`ifdef A
  A_TRUE
\`else
  A_FALSE
\`endif

\`ifndef B
  B_FALSE
\`else
  B_TRUE
\`endif
EOF


cat > "$MY_EXPECT_FILE" <<EOF

  A_TRUE



  B_FALSE

EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" +define+A > "$MY_OUTPUT_FILE" 

status="$?"

[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

cat > "$MY_EXPECT_FILE" <<EOF

  A_TRUE



  B_TRUE

EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" +define+A=a_value+B=b_value > "$MY_OUTPUT_FILE" 

status="$?"

[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

"$preprocessor" preprocess "$MY_INPUT_FILE" +define+A +define+B > "$MY_OUTPUT_FILE" 

status="$?"

[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: including a file with an absolute path"

cat > "$MY_INPUT_FILE" <<EOF
input_content_0
\`include "${MY_ABSOLUTE_INCLUDED_FILE_1}"
input_content_1
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_1" <<EOF
included1_content
EOF


cat > "$MY_EXPECT_FILE" <<EOF
input_content_0
included1_content

input_content_1
EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"

[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: including a file with a relative path with the correct incdir"

cat > "$MY_INPUT_FILE" <<EOF
\`include "${MY_RELATIVE_INCLUDED_FILE_1}"
input_content
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_1" <<EOF
included1_content
EOF


cat > "$MY_EXPECT_FILE" <<EOF
included1_content

input_content
EOF

"$preprocessor" preprocess +incdir+${MY_INCLUDED_FILE_PATH_1} "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"

[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: including a file with a relative path without incdir"

cat > "$MY_INPUT_FILE" <<EOF
\`include "${MY_RELATIVE_INCLUDED_FILE_1}"
input_content
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_1" <<EOF
included1_content
EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"

[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: including nested files with absolute paths"

cat > "$MY_INPUT_FILE" <<EOF
\`include "${MY_ABSOLUTE_INCLUDED_FILE_1}"
input_content
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_1" <<EOF
\`include "${MY_ABSOLUTE_INCLUDED_FILE_2}"
included1_content
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_2" <<EOF
included2_content
EOF

cat > "$MY_EXPECT_FILE" <<EOF
included2_content

included1_content

input_content
EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"

[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: including nested files with relative paths with the correct incdirs"

cat > "$MY_INPUT_FILE" <<EOF
\`include "${MY_RELATIVE_INCLUDED_FILE_1}"
input_content
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_1" <<EOF
\`include "${MY_RELATIVE_INCLUDED_FILE_2}"
included1_content
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_2" <<EOF
included2_content
EOF

cat > "$MY_EXPECT_FILE" <<EOF
included2_content

included1_content

input_content
EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" +incdir+${MY_INCLUDED_FILE_PATH_1}+${MY_INCLUDED_FILE_PATH_2} > "$MY_OUTPUT_FILE" 2>&1

status="$?"

[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: including nested files with relative paths without incdir"

cat > "$MY_INPUT_FILE" <<EOF
\`include "${MY_RELATIVE_INCLUDED_FILE_1}"
input_content
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_1" <<EOF
\`include "${MY_RELATIVE_INCLUDED_FILE_2}"
included1_content
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_2" <<EOF
included2_content
EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 2>&1

status="$?"

[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Line:${LINENO} Test preprocess: including nested files, passing +define+"

cat > "$MY_INPUT_FILE" <<EOF
\`include "${MY_ABSOLUTE_INCLUDED_FILE_1}"
input_content
\`ifdef A
A_TRUE
\`endif
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_1" <<EOF
\`include "${MY_ABSOLUTE_INCLUDED_FILE_2}"
included1_content
\`ifdef A
A_TRUE
\`endif
EOF

cat > "$MY_ABSOLUTE_INCLUDED_FILE_2" <<EOF
included2_content
\`ifdef A
A_TRUE
\`endif
EOF

cat > "$MY_EXPECT_FILE" <<EOF
included2_content

A_TRUE


included1_content

A_TRUE


input_content

A_TRUE

EOF

"$preprocessor" preprocess "$MY_INPUT_FILE" +define+A > "$MY_OUTPUT_FILE" 2>&1

status="$?"

[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "PASS"
