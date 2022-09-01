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
MY_INPUT_FILE="${TEST_TMPDIR}/myinput.txt"
readonly MY_INPUT_FILE
MY_OUTPUT_FILE="${TEST_TMPDIR}/myoutput.txt"
readonly MY_OUTPUT_FILE
MY_EXPECT_FILE="${TEST_TMPDIR}/myexpect.txt"
readonly MY_EXPECT_FILE

# Process script flags and arguments.
[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, verible-verilog-preprocessor path."
  exit 1
}
preprocessor="$(rlocation ${TEST_WORKSPACE}/$1)"

################################################################################
echo "=== Test no command."

"$preprocessor" > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Test invalid command."

"$preprocessor" bad-subcommand > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################
echo "=== Test the 'help' command."

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
echo "=== Test 'help' on a specific command."

"$preprocessor" help help > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 1
}

################################################################################
echo "=== Test strip-comments: missing file argument."

"$preprocessor" strip-comments > /dev/null

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 1
}

################################################################################################################################################################
echo "=== Test strip-comments: white out comments"

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
echo "=== Test strip-comments: delete comments"

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
echo "=== Test strip-comments: replace comments"

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
echo "=== Test strip-comments: on a lexically invalid source file"

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
  exit 0
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
echo "=== Test multiple-compilation-unit: on a source file with conditionals"

cat > "$MY_INPUT_FILE" <<EOF
\`include "file.sv"
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
(#259: "\`include")
(#313: ""file.sv"")
(#359: "module")
(#293: "m")
(#40: "(")
(#41: ")")
(#59: ";")
(#417: "wire")
(#293: "x")
(#61: "=")
(#300: "1")
(#59: ";")
(#379: "real")
(#293: "is_defined")
(#61: "=")
(#300: "0")
(#59: ";")
(#336: "endmodule")

EOF

"$preprocessor" multiple-compilation-unit "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE"

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 0
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Test generate-variants: on a source file with nested conditionals"

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
  exit 0
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Test generate-variants: with limited variants"

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
  exit 0
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "=== Test generate-variants: with invalid conditionals"

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
  exit 0
}

################################################################################
echo "=== Test generate-variants: with multiple files"

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
  exit 0
}

################################################################################
echo "=== Test multiple-compilation-unit: with missing files"

"$preprocessor" multiple-compilation-unit > "$MY_OUTPUT_FILE" 2>&1

status="$?"
[[ $status == 1 ]] || {
  "Expected exit code 1, but got $status"
  exit 0
}

################################################################################
echo "=== Test generate-variants: with multiple files"

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
  exit 0
}

################################################################################
echo "=== Test multiple-compilation-unit: with multiple files"

cat > "$MY_INPUT_FILE" <<EOF
\`ifdef A
  A_TRUE
\`else
  A_FALSE
\`endif
EOF


cat > "$MY_EXPECT_FILE" <<EOF
(#293: "A_FALSE")

(#293: "A_FALSE")

EOF

"$preprocessor" multiple-compilation-unit "$MY_INPUT_FILE" "$MY_INPUT_FILE" > "$MY_OUTPUT_FILE" 

status="$?"
[[ $status == 0 ]] || {
  "Expected exit code 0, but got $status"
  exit 0
}

diff --strip-trailing-cr -u "$MY_EXPECT_FILE" "$MY_OUTPUT_FILE" || {
  exit 1
}

################################################################################
echo "PASS"
