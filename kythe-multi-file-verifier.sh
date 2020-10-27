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

Extracts Kythe facts from the given verilog files and runs Kythe verifier on the produced facts.
EOF
}

set -o pipefail
KYTHE_BINDIR="/opt/kythe/tools"
KYTHE_OUT="./kythe-out"

# Paths for multi_file_test.
VERILOG_MULTI_FILE_TEST_DIR="./verilog/tools/kythe/testdata/more_testdata/multi_file_test"
VERILOG_MULTI_FILE_TEST_FILE_LIST="${VERILOG_MULTI_FILE_TEST_DIR}/file_list.txt"
VERILOG_MULTI_FILE_TEST_FILES="${VERILOG_MULTI_FILE_TEST_DIR}/*.sv"

# Paths for include_file_test.
VERILOG_INCLUDE_FILE_TEST_DIR="./verilog/tools/kythe/testdata/more_testdata/include_file_test"
VERILOG_INCLUDE_FILE_TEST_FILE_LIST="${VERILOG_INCLUDE_FILE_TEST_DIR}/file_list.txt"
VERILOG_INCLUDE_FILE_TEST_FILES="${VERILOG_INCLUDE_FILE_TEST_DIR}/*.sv*"

# Paths for include_dir_test.
VERILOG_INCLUDE_DIR_TEST_DIR="./verilog/tools/kythe/testdata/more_testdata/include_with_dir_test"
VERILOG_INCLUDE_DIR_TEST_INCLUDE_DIR_PATH="${VERILOG_INCLUDE_DIR_TEST_DIR}/include_dir"
VERILOG_INCLUDE_DIR_TEST_FILE_LIST="${VERILOG_INCLUDE_DIR_TEST_DIR}/file_list.txt"
VERILOG_INCLUDE_DIR_TEST_FILES="${VERILOG_INCLUDE_DIR_TEST_DIR}/*.sv ${VERILOG_INCLUDE_FILE_TEST_DIR}/A.svh ${VERILOG_INCLUDE_FILE_TEST_DIR}/B.svh ${VERILOG_INCLUDE_FILE_TEST_DIR}/C.sv ${VERILOG_INCLUDE_DIR_TEST_INCLUDE_DIR_PATH}/*.svh"

# You can find prebuilt binaries at https://github.com/kythe/kythe/releases.
# This script assumes that they are installed to /opt/kythe.
bazel build -c opt //verilog/tools/kythe:all

# Test include_with_dir.
bazel-bin/verilog/tools/kythe/verible-verilog-kythe-extractor "${VERILOG_INCLUDE_DIR_TEST_FILE_LIST}" --printkythefacts --include_dir_paths "${VERILOG_INCLUDE_FILE_TEST_DIR} ${VERILOG_INCLUDE_DIR_TEST_INCLUDE_DIR_PATH}" > "${KYTHE_OUT}"/entries

${KYTHE_BINDIR}/entrystream --read_format=json < "${KYTHE_OUT}"/entries \
| ${KYTHE_BINDIR}/verifier ${VERILOG_INCLUDE_DIR_TEST_FILES}

# Test ordered multi-files.
bazel-bin/verilog/tools/kythe/verible-verilog-kythe-extractor "${VERILOG_MULTI_FILE_TEST_FILE_LIST}" --printkythefacts > "${KYTHE_OUT}"/entries

${KYTHE_BINDIR}/entrystream --read_format=json < "${KYTHE_OUT}"/entries \
| ${KYTHE_BINDIR}/verifier ${VERILOG_MULTI_FILE_TEST_FILES}

# Test include_file.
bazel-bin/verilog/tools/kythe/verible-verilog-kythe-extractor "${VERILOG_INCLUDE_FILE_TEST_FILE_LIST}" --printkythefacts > "${KYTHE_OUT}"/entries

${KYTHE_BINDIR}/entrystream --read_format=json < "${KYTHE_OUT}"/entries \
| ${KYTHE_BINDIR}/verifier ${VERILOG_INCLUDE_FILE_TEST_FILES}
