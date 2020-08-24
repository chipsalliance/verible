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
$0 verilog file...

Extracts Kythe facts from the given verilog file and runs Kythe verifier on the produced facts.
EOF
}

set -o pipefail
KYTHE_BINDIR="/opt/kythe/tools"
# You can find prebuilt binaries at https://github.com/kythe/kythe/releases.
# This script assumes that they are installed to /opt/kythe.
bazel build //verilog/tools/kythe:all
bazel-bin/verilog/tools/kythe/verible-verilog-kythe-extractor "$1"  --printkythefacts > entries
# Read JSON entries from standard in to a graphstore.
${KYTHE_BINDIR}/entrystream --read_format=json < entries \
  | ${KYTHE_BINDIR}/verifier "$1" --annotated_graphviz > foo.dot
xdot foo.dot

