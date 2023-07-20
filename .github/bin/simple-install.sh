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

BASE_DIR=bazel-bin/verilog/tools

for f in diff/verible-verilog-diff \
           formatter/verible-verilog-format \
           kythe/verible-verilog-kythe-extractor \
           lint/verible-verilog-lint \
           ls/verible-verilog-ls \
           obfuscator/verible-verilog-obfuscate \
           preprocessor/verible-verilog-preprocessor \
           project/verible-verilog-project \
           syntax/verible-verilog-syntax
do
  install "${BASE_DIR}/$f" "${TARGET_DIR}"
done
