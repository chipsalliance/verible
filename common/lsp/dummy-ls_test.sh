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
# Simple end-to-end test
awk '{printf("Content-Length: %d\r\n\r\n%s", length($0), $0)}' > ${TMP_IN} <<EOF
{"jsonrpc":"2.0","method":"initialize","params":null,"id":1}
{"jsonrpc":"2.0","method":"shutdown","params":{},"id":2}
EOF

cat > "${JSON_EXPECTED}" <<EOF
[
  {
    "json_contains": {
        "id":1,
        "result": {
          "serverInfo": {"name" : "Verible testing language server."},
          "capabilities": { "ignored_property":[1,2,3] }
        }
    }
  },
  {
    "json_contains": { "id":2, "result":null }
  }
]
EOF

"${LSP_SERVER}" < ${TMP_IN} 2> "${MSG_OUT}" \
  | ${JSON_RPC_EXPECT} ${JSON_EXPECTED}

JSON_RPC_EXIT=$?

if [ $JSON_RPC_EXIT -ne 0 ]; then
   echo "Exit code of json rpc expect; first error at $JSON_RPC_EXIT"
   exit 1
fi

echo "-- stderr messages --"
cat ${MSG_OUT}

grep "shutdown request" "${MSG_OUT}" > /dev/null
if [ $? -ne 0 ]; then
  echo "Didn't get shutdown feedback"
  exit 1
fi
