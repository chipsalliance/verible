#!/usr/bin/env bash -e
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
$0 PATH DIR INCLUDE_DIRs

PATH is the path to the file list.
DIR is the file list root.
INCLUDE_DIRs the directories for includes.

Extracts Kythe facts from the given verilog file list and run Kythe web ui to visualize code navigation.
This script assumes that the script is to be run from Verible project root.
This script assumes that Kythe binaries are installed to /opt/kythe.
EOF
}

set -o pipefail
BROWSE_PORT="${BROWSE_PORT:-8080}"
KYTHE_BINDIR="/opt/kythe/tools"
KYTHE_OUT="./kythe-out"

[[ "$#" == 3 ]] || { usage; exit 1; }
[[ -x "$KYTHE_BINDIR"/entrystream ]] || { echo "Missing kythe tool: ..." ; exit 1; }

# You can find prebuilt binaries at https://github.com/kythe/kythe/releases.
# This script assumes that they are installed to /opt/kythe.
# If you build the tools yourself or install them to a different location,
# make sure to pass the correct public_resources directory to http_server.
rm -f -- ${KYTHE_OUT}/graphstore/* ${KYTHE_OUT}/tables/*
mkdir -p ${KYTHE_OUT}/graphstore ${KYTHE_OUT}/tables
bazel build -c opt //verible/verilog/tools/kythe:all

# Read JSON entries from standard in to a graphstore.
bazel-bin/verible/verilog/tools/kythe/verible-verilog-kythe-extractor --file_list_path "$1" --file_list_root "$2" --print_kythe_facts json --include_dir_paths "$3" > "${KYTHE_OUT}"/entries
# Write entry stream into a GraphStore
"${KYTHE_BINDIR}"/entrystream --read_format=json < "${KYTHE_OUT}"/entries \
| "${KYTHE_BINDIR}"/write_entries -graphstore "${KYTHE_OUT}"/graphstore

# Convert the graphstore to serving tables.
"${KYTHE_BINDIR}"/write_tables -graphstore "${KYTHE_OUT}"/graphstore -out="${KYTHE_OUT}"/tables
# Host the browser UI.
"${KYTHE_BINDIR}"/http_server -serving_table "${KYTHE_OUT}"/tables \
  -public_resources="/opt/kythe/web/ui" \
  -listen="localhost:${BROWSE_PORT}"  # ":${BROWSE_PORT}" allows access from other machines
