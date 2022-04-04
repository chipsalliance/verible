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

# Testing a file with syntax errors: this should output some diagnostic
{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file://syntaxerror.sv","text":"brokenfile\n"}}}

# Let's manually request these diagnostics
{"jsonrpc":"2.0", "id":2, "method":"textDocument/diagnostic","params":{"textDocument":{"uri":"file://syntaxerror.sv"}}}

# A file with a lint error (no newline at EOF). Then editing it and watching diagnostic go away.
{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file://mini.sv","text":"module mini();\nendmodule"}}}

# Requesting a code-action exactly at the position the EOF message is reported.
# This is an interesting special case, as the missing EOF-newline is an empty
# range, yet it should be detected as overlapping with that diagnostic message.
{"jsonrpc":"2.0", "id":10, "method":"textDocument/codeAction","params":{"textDocument":{"uri":"file://mini.sv"},"range":{"start":{"line":1,"character":9},"end":{"line":1,"character":9}}}}
{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file://mini.sv"},"contentChanges":[{"range":{"start":{"character":9,"line":1},"end":{"character":9,"line":1}},"text":"\n"}]}}
{"jsonrpc":"2.0", "id":11, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://mini.sv"}}}
{"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"file://mini.sv"}}}

# Attempt to query closed file should gracefully return an empty response.
{"jsonrpc":"2.0", "id":12, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://mini.sv"}}}

# Highlight, but only symbols not highlighting 'assign' non-symbol.
{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file://sym.sv","text":"module sym();\nassign a=1;assign b=a+1;endmodule\n"}}}
{"jsonrpc":"2.0", "id":20, "method":"textDocument/documentHighlight","params":{"textDocument":{"uri":"file://sym.sv"},"position":{"line":1,"character":7}}}
{"jsonrpc":"2.0", "id":21, "method":"textDocument/documentHighlight","params":{"textDocument":{"uri":"file://sym.sv"},"position":{"line":1,"character":2}}}

# Formatting a file
{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file://fmt.sv","text":"module fmt();\nassign a=1;\nassign b=2;endmodule\n"}}}
{"jsonrpc":"2.0", "id":30, "method":"textDocument/rangeFormatting","params":{"textDocument":{"uri":"file://fmt.sv"},"range":{"start":{"line":1,"character":0},"end":{"line":2,"character":0}}}}
{"jsonrpc":"2.0", "id":31, "method":"textDocument/rangeFormatting","params":{"textDocument":{"uri":"file://fmt.sv"},"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":1}}}}
{"jsonrpc":"2.0", "id":32, "method":"textDocument/rangeFormatting","params":{"textDocument":{"uri":"file://fmt.sv"},"range":{"start":{"line":2,"character":0},"end":{"line":2,"character":1}}}}
{"jsonrpc":"2.0", "id":33, "method":"textDocument/rangeFormatting","params":{"textDocument":{"uri":"file://fmt.sv"},"range":{"start":{"line":1,"character":0},"end":{"line":3,"character":0}}}}
{"jsonrpc":"2.0", "id":34, "method":"textDocument/formatting","params":{"textDocument":{"uri":"file://fmt.sv"}}}

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
          "uri": "file://syntaxerror.sv",
          "diagnostics":[{"message":"syntax error"}]
       }
     }
  },
  {
    "json_contains": {
        "id":2,
        "result": {
           "kind":"full",
           "items":[{"message":"syntax error"}]
        }
       }
  },

  {
    "json_contains": {
       "method":"textDocument/publishDiagnostics",
       "params": {
          "uri": "file://mini.sv",
          "diagnostics":[{"message":"File must end with a newline.","range":{"start":{"line":1,"character":9}}}]
       }
    }
  },
  {
    "json_contains": {
       "id":10,
       "result": [
          {"edit": {"changes": {"file://mini.sv":[{"newText":"\n"}]}}}
        ]
    }
  },
  {
    "json_contains": {
       "method":"textDocument/publishDiagnostics",
       "params": {
          "uri": "file://mini.sv",
          "diagnostics":[]
       }
    }
  },
  {
    "json_contains": {
       "id":11,
       "result": [
          {"kind":6, "name":"mini"}
        ]
    }
  },
  {
    "json_contains":{
       "id":12  ,
       "result": []
    }
  },


  {
    "json_contains": {
       "method":"textDocument/publishDiagnostics",
       "params": {
          "uri": "file://sym.sv",
          "diagnostics":[]
       }
    }
  },
  {
    "json_contains":{
       "id":20,
       "result": [
          {"range":{"start":{"line":1, "character": 7}, "end":{"line":1, "character": 8}}},
          {"range":{"start":{"line":1, "character":20}, "end":{"line":1, "character":21}}}
        ]
    }
  },
  {
    "json_contains":{
       "id":21,
       "result": []
    }
  },

  {
    "json_contains": {
       "method":"textDocument/publishDiagnostics",
       "params": {
          "uri": "file://fmt.sv",
          "diagnostics":[]
       }
    }
  },
  {
    "json_contains":{
       "id":30,
       "result": [
           {"newText":"  assign a=1;\n","range":{"end":{"character":0,"line":2},"start":{"character":0,"line":1}}}
        ]
    }
  },
  {
    "json_contains":{
       "id":31,
       "result": [
           {"newText":"  assign a=1;\n","range":{"end":{"character":0,"line":2},"start":{"character":0,"line":1}}}
        ]
    }
  },
  {
    "json_contains":{
       "id":32,
       "result": [
           {"newText":"  assign b=2;\nendmodule\n","range":{"end":{"character":0,"line":3},"start":{"character":0,"line":2}}}
        ]
    }
  },
  {
    "json_contains":{
       "id":33,
       "result": [
           {"newText":"  assign a = 1;\n  assign b = 2;\nendmodule\n","range":{"end":{"character":0,"line":3},"start":{"character":0,"line":1}}}
        ]
    }
  },
  {
    "json_contains":{
       "id":34,
       "result": [{
           "newText": "module fmt ();\n  assign a = 1;\n  assign b = 2;\nendmodule\n",
           "range":   {"end":{"character":0,"line":3},"start":{"character":0,"line":0}}
        }]
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
