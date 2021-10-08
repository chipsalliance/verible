#!/bin/bash
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

[[ "$#" == 1 ]] || {
  echo "Expecting 1 positional argument, dummy-lsp path."
  exit 1
}
DUMMY_LSP="$(rlocation ${TEST_WORKSPACE}/$1)"

TMP_IN=${TEST_TMPDIR:-/tmp/}/test-lsp-in.txt
TMP_OUT=${TEST_TMPDIR:-/tmp/}/test-lsp-out.txt

# One message per line, converted by the awk script to header/body.
# Simple end-to-end test
awk '{printf("Content-Length: %d\r\n\r\n%s", length($0), $0)}' > ${TMP_IN} <<EOF
{"jsonrpc":"2.0","method":"initialize","params":null,"id":1}
{"jsonrpc":"2.0","method":"shutdown","params":{},"id":2}
EOF

"${DUMMY_LSP}" < ${TMP_IN} > "${TMP_OUT}"

cat ${TMP_OUT}

# Check if an expected content is returned
grep "testing language server" "${TMP_OUT}" > /dev/null

exit $?
