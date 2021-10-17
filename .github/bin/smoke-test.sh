#!/bin/bash
# Copyright 2021 The Verible Authors.
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

###
# Simple smoke test by running all Verible tools over various code-bases
# that can be fetched by git and making sure it does not crash, letting
# out the magic smoke.
#
# We explicitly do _not_ look if the tools fail with some other exit
# code, as we don't care if e.g. there are lint issues in the projects
# at hand, just that Verible is not crashing on input.
#
# Once we are able to parse full of system verilog, we should check
# that verible-verilog-syntax always return 0.
# For now, even  if there are syntax errors reported: as long as it doesn't
# crash Verible, we're good.
###

set -u

# Build all the installable binaries that we're going to use below.
bazel build :install-binaries
readonly BINARY_BASE_DIR=bazel-bin/verilog/tools

VERIBLE_TOOLS_TO_RUN="syntax/verible-verilog-syntax \
              lint/verible-verilog-lint \
              formatter/verible-verilog-format"

# verible-verilog-project crashes on some projects right now, and since
# it is not widely used yet, exclude it for now. The following bugs need
# to be fixe before it can be put in the VERIBLE_TOOLS_TO_RUN list above
#
#  * Issue #917  https://github.com/chipsalliance/verible/issues/917
#  * Issue #1002 https://github.com/chipsalliance/verible/issues/1002
#  * Issue #1003 https://github.com/chipsalliance/verible/issues/1003
#VERIBLE_TOOLS_TO_RUN+=" project/verible-verilog-project"

# A few projects that can be fetched from git and represent a good
# cross-section of different styles.
#
# We run all Verible tools on these.
#
# Not included: https://github.com/google/skywater-pdk (the recursive
# checkout is gigantic. And the 40k+ files would not fit in one
# commandline. But it would actually pass smoke test)
#
# TODO: the following were tested but crash the formatter, so are currently
# _not_ included but should be as soon as the associated Verible bugs are fixed.
# ------ github url -------------------------------|-- verible issues -----
# https://github.com/black-parrot/black-parrot          #1004
# https://github.com/lowRISC/opentitan                  #1005 #1006 #1007 #1008
# https://github.com/steveicarus/ivtest                 #1010
# https://github.com/bespoke-silicon-group/basejump_stl #1011 #1012
readonly TEST_GIT_PROJECTS="https://github.com/lowRISC/ibex \
         https://github.com/chipsalliance/Cores-SweRV \
         https://github.com/openhwgroup/cva6 \
         https://github.com/SymbiFlow/uvm \
         https://github.com/taichi-ishitani/tnoc \
         https://github.com/ijor/fx68k \
         https://github.com/jamieiles/80x86 \
         https://github.com/SymbiFlow/XilinxUnisimLibrary"

# Run smoke test on provided files for project.
# Returns 0 if all tools finished without crashing.
#
# First parameter : project name
# Second parameter: name of file containing a list of {System}Verilog files
function run_smoke_test() {
  local PROJECT_NAME=$1
  local FILELIST=$2
  local result=0

  echo "== Running verible on $PROJECT_NAME with $(cat $FILELIST | wc -l) files"

  for tool in $VERIBLE_TOOLS_TO_RUN ; do
    EXTRA_PARAM=""
    INPUT_FILES="$(cat ${FILELIST})"
    case $tool in
      *-format) EXTRA_PARAM="--inplace" ;;
      *-project)
        INPUT_FILES=""
        EXTRA_PARAM="symbol-table-defs --file_list_root=/ --file_list_path ${FILELIST}" ;;
    esac

    ${BINARY_BASE_DIR}/${tool} ${EXTRA_PARAM} ${INPUT_FILES} >/dev/null 2>&1
    local EXIT_CODE=$?

    printf "%-20s %-32s\n" ${PROJECT_NAME} ${tool}

    # A regular error exit code we accept as normal operation of the tool if it
    # encountered a syntax error. Here, we are only interested in not receiving
    # a signal such as an abort or segmentation fault.
    # So we check for 126, 127 (command not found or executable)
    # and >= 128 (128 + signal-number)
    # https://www.gnu.org/software/bash/manual/html_node/Exit-Status.html
    if [ $EXIT_CODE -ge 126 ]; then
      echo " -> Got exit code $EXIT_CODE for $tool, indicating crash."
      result=1
    fi
  done
  return ${result}
}

readonly BASE_TEST_DIR=/tmp/test/verible-smoke-test
mkdir -p $BASE_TEST_DIR
trap 'rm -rf -- "$BASE_TEST_DIR"' EXIT

status_sum=0

for git_project in ${TEST_GIT_PROJECTS} ; do
  PROJECT_NAME=$(basename $git_project)
  PROJECT_DIR=${BASE_TEST_DIR}/${PROJECT_NAME}
  git clone -q ${git_project} ${PROJECT_DIR} 2>/dev/null

  # Just collect everything that looks like Verilog/SystemVerilog
  FILELIST=${PROJECT_DIR}/verible.filelist
  find ${PROJECT_DIR} -name "*.sv" -o -name "*.svh" -o -name "*.v" > ${FILELIST}

  run_smoke_test ${PROJECT_NAME} ${FILELIST}
  status_sum=$((${status_sum} + $?))
  echo
done

echo "Sum of issues ${status_sum}"
exit ${status_sum}
