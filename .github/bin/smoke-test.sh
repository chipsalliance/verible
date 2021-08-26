#!/bin/bash
# Simple smoke test by running the formatter over some code-base
# making sure it dos not crash

bazel build verilog/tools/formatter:verible-verilog-format

IBEX_DIR=/tmp/test/ibex
git clone https://github.com/lowrisc/ibex ${IBEX_DIR}

bazel-bin/verilog/tools/formatter/verible-verilog-format --inplace $(find ${IBEX_DIR} -name "*.sv")

# A regular error exit code we accept as normal operation of the tool if it
# encountered a syntax error. Here, we are only interested in not receiving
# a signal such as an abort or segmentation fault.
# So we check for 126, 127 (command not found or executable) and >= 128 (signal)
# https://www.gnu.org/software/bash/manual/html_node/Exit-Status.html
EXIT_CODE=$?
if [ $EXIT_CODE -ge 126 ]; then
  echo "Got exit code $EXIT_CODE"
  exit 1
fi
