#!/bin/bash -e
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

