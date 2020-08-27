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
# You can find prebuilt binaries at https://github.com/kythe/kythe/releases.
# This script assumes that they are installed to /opt/kythe.
# If you build the tools yourself or install them to a different location,
# make sure to pass the correct public_resources directory to http_server.
rm -f -- graphstore/* tables/*
mkdir -p graphstore tables
bazel build //verilog/tools/kythe:all
bazel-bin/verilog/tools/kythe/verible-verilog-kythe-extractor "$1"  --printkythefacts > entries
# Read JSON entries from standard in to a graphstore.
${KYTHE_BINDIR}/entrystream --read_format=json < entries \
  | ${KYTHE_BINDIR}/write_entries -graphstore graphstore
# Convert the graphstore to serving tables.
${KYTHE_BINDIR}/write_tables -graphstore graphstore -out=tables
# Host the browser UI.
${KYTHE_BINDIR}/http_server -serving_table tables \
  -public_resources="/opt/kythe/web/ui" \
  -listen="localhost:${BROWSE_PORT}"  # ":${BROWSE_PORT}" allows access from other machines

