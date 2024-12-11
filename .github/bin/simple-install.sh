#!/usr/bin/env bash
# Simple install script, as MacOS install is not working with our install target
#
# Should be consolidated with githhub-releases-setup.sh, but for now keeping it
# simple to run on limited platforms.
##

set -e

if [ $# -ne 1 ]; then
  echo "usage $0 <target-dir>"
  exit 1
fi

TARGET_DIR=$1
mkdir -p "${TARGET_DIR}"

# Requires to have built before with
#  bazel build :install-binaries

# Could we get the list of source from bazel query somehow ?

TOOLS_DIR=bazel-bin/verible/verilog/tools
for f in diff/verible-verilog-diff \
           formatter/verible-verilog-format \
           kythe/verible-verilog-kythe-extractor \
           kythe/verible-verilog-kythe-kzip-writer \
           lint/verible-verilog-lint \
           ls/verible-verilog-ls \
           obfuscator/verible-verilog-obfuscate \
           preprocessor/verible-verilog-preprocessor \
           project/verible-verilog-project \
           syntax/verible-verilog-syntax
do
  install "${TOOLS_DIR}/$f" "${TARGET_DIR}"
done

COMMON_TOOLS_DIR=bazel-bin/verible/common/tools
for f in verible-patch-tool
do
  install "${COMMON_TOOLS_DIR}/$f" "${TARGET_DIR}"
done
