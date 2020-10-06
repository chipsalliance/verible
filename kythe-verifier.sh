#!/bin/bash -e
# Copyright 2020 The Verible Authors.
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

usage() {
  cat <<EOF

Extracts Kythe facts from the given verilog file and runs Kythe verifier on the produced facts.
EOF
}

set -o pipefail
KYTHE_BINDIR="/opt/kythe/tools"
KYTHE_OUT="./kythe-out"

# The files are expected to be self-contained single-file test cases.
VERILOG_TEST_FILES="./verilog/tools/kythe/testdata/more_testdata/*.sv"
# You can find prebuilt binaries at https://github.com/kythe/kythe/releases.
# This script assumes that they are installed to /opt/kythe.
bazel build //verilog/tools/kythe:all

for i in $VERILOG_TEST_FILES; do
  # Read JSON entries from standard in to a graphstore.
  bazel-bin/verilog/tools/kythe/verible-verilog-kythe-extractor "$i"  --printkythefacts > ${KYTHE_OUT}/entries

  ${KYTHE_BINDIR}/entrystream --read_format=json < ${KYTHE_OUT}/entries \
  | ${KYTHE_BINDIR}/verifier "$i"
done
