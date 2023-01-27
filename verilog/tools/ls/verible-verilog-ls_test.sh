#!/usr/bin/env bash
# Copyright 2021 The Verible Authors.
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

# This test provides an end-to-end test for the server.
# Simple round-trip
#  - 1: initialize
#  - event: open file
#  - 2: request document symbol on that file
#  - event: close file
#  - 3: attempt to request documenet symbol on that closed file.
#  - 100: shutdown.
#
#
# Functional tests should go to verilog-language-server_test.cc

set -u

[[ "$#" == 2 ]] || {
  echo "Expecting 2 positional arguments: lsp-server json-rpc-expect"
  exit 1
}
LSP_SERVER="$(rlocation ${TEST_WORKSPACE}/$1)"
JSON_RPC_EXPECT="$(rlocation ${TEST_WORKSPACE}/$2)"

TMP_IN=${TEST_TMPDIR:-/tmp/}/test-lsp-in.txt
JSON_EXPECTED=${TEST_TMPDIR:-/tmp/}/test-lsp-json-expect.txt

MSG_OUT=${TEST_TMPDIR:-/tmp/}/test-lsp-out-msg.txt

# One message per line, converted by the awk script to header/body.

# Starting up server, sending two files, a file with a parse error and
# a file that parses, but has a EOF newline linting diagnostic.
#
# We get the diagnostic messages for both of these files, one reporting
# a syntax error, one reporting a lint error.
#
# We then modify the second file and edit the needed newline at the end of
# the buffer, and then get an update with zero diagnostic errors back.
#
# TODO: maybe this awk-script should be replaced with something that allows
# multi-line input with comment.
awk '/^{/ { printf("Content-Length: %d\r\n\r\n%s", length($0), $0)}' > ${TMP_IN} <<EOF
{"jsonrpc":"2.0", "id":1, "method":"initialize","params":null}

# Testing a file that should not create diagnostics
{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file://simple.sv","text":"module simple();\nendmodule\n"}}}

# Request document outline
{"jsonrpc":"2.0", "id":2, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://simple.sv"}}}

# Close the file
{"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"file://simple.sv"}}}

# Attempt to query closed file should gracefully return an empty response.
{"jsonrpc":"2.0", "id":3, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://simple.sv"}}}

{"jsonrpc":"2.0", "id":100, "method":"shutdown","params":{}}
EOF

# TODO: change json rpc expect to allow comments in the input.
cat > "${JSON_EXPECTED}" <<EOF
[
  {
    "json_contains": {
        "id":1,
        "result": {
          "serverInfo": {"name" : "Verible Verilog language server."}
        }
    }
  },


  {
    "json_contains": {
       "method":"textDocument/publishDiagnostics",
       "params": {
          "uri": "file://simple.sv",
          "diagnostics":[]
       }
     }
  },

  {
    "json_contains": {
       "id":2,
       "result": [
          {"kind":6, "name":"simple"}
        ]
    }
  },

  {
    "json_contains":{
       "id":3,
       "result": []
    }
  },

  {
    "json_contains": { "id":100 }
  }
]
EOF

"${LSP_SERVER}" < ${TMP_IN} 2> "${MSG_OUT}" \
  | ${JSON_RPC_EXPECT} ${JSON_EXPECTED}

JSON_RPC_EXIT=$?

echo "-- stderr messages --"
cat ${MSG_OUT}

if [ $JSON_RPC_EXIT -ne 0 ]; then
  # json-rpc-expect outputs the entry, where the mismatch occured, in exit code
  echo "Exit code of json rpc expect; first error at $JSON_RPC_EXIT"
  exit 1
fi

grep "shutdown request" "${MSG_OUT}" > /dev/null
if [ $? -ne 0 ]; then
  echo "Didn't get shutdown feedback"
  exit 1
fi
