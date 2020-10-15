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

Extracts Kythe facts from the given verilog file and run Kythe web ui to visualize code navigation.
EOF
}

set -o pipefail
BROWSE_PORT="${BROWSE_PORT:-8080}"
KYTHE_BINDIR="/opt/kythe/tools"
KYTHE_OUT="./kythe-out"
VERILOG_TEST_FILE_LIST="./verilog/tools/kythe/testdata/more_testdata/file_list.txt"
# You can find prebuilt binaries at https://github.com/kythe/kythe/releases.
# This script assumes that they are installed to /opt/kythe.
# If you build the tools yourself or install them to a different location,
# make sure to pass the correct public_resources directory to http_server.
rm -f -- ${KYTHE_OUT}/graphstore/* ${KYTHE_OUT}/tables/*
mkdir -p ${KYTHE_OUT}/graphstore ${KYTHE_OUT}/tables
bazel build -c opt //verilog/tools/kythe:all

for i in "$@"; do
  echo "$(basename $i)" > "$(dirname $i)/file_name.txt"
  cat "$(dirname $i)/file_name.txt"
  # Read JSON entries from standard in to a graphstore.
  bazel-bin/verilog/tools/kythe/verible-verilog-kythe-extractor "$(dirname $i)/file_name.txt"  --printkythefacts > "${KYTHE_OUT}"/entries
  # Write entry stream into a GraphStore
  "${KYTHE_BINDIR}"/entrystream --read_format=json < "${KYTHE_OUT}"/entries \
  | "${KYTHE_BINDIR}"/write_entries -graphstore "${KYTHE_OUT}"/graphstore

  rm "$(dirname $i)/file_name.txt"
done

# Convert the graphstore to serving tables.
"${KYTHE_BINDIR}"/write_tables -graphstore "${KYTHE_OUT}"/graphstore -out="${KYTHE_OUT}"/tables
# Host the browser UI.
"${KYTHE_BINDIR}"/http_server -serving_table "${KYTHE_OUT}"/tables \
  -public_resources="/opt/kythe/web/ui" \
  -listen="localhost:${BROWSE_PORT}"  # ":${BROWSE_PORT}" allows access from other machines
