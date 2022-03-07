#!/bin/bash
# -*- mode: sh; sh-basic-offset: 2; indent-tabs-mode: nil; -*-
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

# Suppress '... aborted' messages bash would print when a tool crashes.
# Comment out to see syntax errors in bash while working on script.
exec 2>/dev/null

set -u   # Be strict: only allow using a variable after it is assigned

TMPDIR="${TMPDIR:-/tmp}"
readonly BASE_TEST_DIR=${TMPDIR}/test/verible-smoke-test

# Some terminal codes to highlight
readonly TERM_RED=$'\033[1;31m'
readonly TERM_BOLD=$'\033[1m'
readonly TERM_RESET=$'\033[0m'

# Build all the installable binaries that we're going to use below.
# TODO: Consider running with
#  * address sanitizer (best result with libc++). CAVE: slow
#  * running with -D_GLIBCXX_DEBUG (also see #1056)
bazel build :install-binaries

readonly BINARY_BASE_DIR=bazel-bin/verilog/tools

# In case the binaries are run with ASAN:
# By default, failing asan binaries exit with exit code 1.
# Let's change it to something that we can distinguish from 'normal operation'
export ASAN_OPTIONS="exitcode=140"

readonly VERIBLE_TOOLS_TO_RUN="syntax/verible-verilog-syntax \
                               lint/verible-verilog-lint \
                               formatter/verible-verilog-format \
                               project/verible-verilog-project \
                               kythe/verible-verilog-kythe-extractor"

# A few projects that can be fetched from git and represent a good
# cross-section of different styles.
#
# We run all Verible tools on these.
#
# Not included: https://github.com/google/skywater-pdk (the recursive
# checkout is gigantic.)
#
# There are some known issues which are all recorded in the associative
# array below, mapping them to Verible issue tracker numbers.
readonly TEST_GIT_PROJECTS="https://github.com/lowRISC/ibex \
         https://github.com/lowRISC/opentitan \
         https://github.com/chipsalliance/Cores-SweRV \
         https://github.com/openhwgroup/cva6 \
         https://github.com/SymbiFlow/uvm \
         https://github.com/taichi-ishitani/tnoc \
         https://github.com/ijor/fx68k \
         https://github.com/jamieiles/80x86 \
         https://github.com/SymbiFlow/XilinxUnisimLibrary \
         https://github.com/black-parrot/black-parrot
         https://github.com/steveicarus/ivtest \
         https://github.com/bespoke-silicon-group/basejump_stl"

##
# Some of the files in the projects will have issues.
# We record each of them with the associated bug number in this set so that
# we can locally waive the issue and continue with more files,
# but also have it documented which are affected.
#
# Any new issues that arise should be recorded in the verible bug-tracker to
# be fixed.
# Goal: The following list shall be empty :)
declare -A KnownIssue

#--- Opentitan
KnownIssue[formatter:$BASE_TEST_DIR/opentitan/hw/ip/aes/dv/aes_model_dpi/aes_model_dpi_pkg.sv]=1006
# There is also bug 1008 which only shows up if compiled with asan

#--- ivtest
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/stask_sens_null_arg.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/sv_port_default14.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/pr1763333.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/blankport.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/param_times.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/sv_default_port_value1.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/sv_default_port_value3.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/pr2202846c.v]=1015
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/packed_dims_invalid_class.v]=1146
KnownIssue[project:$BASE_TEST_DIR/ivtest/ivltests/wreal.v]=1017

#--- Basejump
# These mostly crash for all the same reason except the first.

#--- Blackparrot

#--- Too many to mention manually, so here we do the 'waive all' approach
declare -A KnownProjectToolIssue
KnownProjectToolIssue[project:basejump_stl]="#1255"
KnownProjectToolIssue[project:ibex]="#1255"
KnownProjectToolIssue[project:uvm]="#1255"
KnownProjectToolIssue[project:opentitan]="#1255"

# Run smoke test on provided files for project.
# Returns 0 if all tools finished without crashing.
#
# First parameter : project name
# Second parameter: name of file containing a list of {System}Verilog files
function run_smoke_test() {
  local PROJECT_FILE_LIST=${TMPDIR}/filelist.$$.list
  local TOOL_OUT=${TMPDIR}/tool.$$.out
  local PROJECT_NAME=$1
  local FILELIST=$2
  local GIT_URL=$3
  local NUM_FILES=$(wc -l < ${FILELIST})
  local result=0

  echo "::group::== Running verible on ${TERM_BOLD}${PROJECT_NAME}${TERM_RESET} with ${NUM_FILES} files =="

  for tool in $VERIBLE_TOOLS_TO_RUN ; do
    printf "%-20s %-32s\n" ${PROJECT_NAME} ${tool}
    local short_tool_name=$(dirname ${tool})
    local non_zero_exit_code=0

    while read single_file; do
      # TODO(hzeller) the project tool and kythe extractor are meant to run
      # on a bunch of files not individual files. Create a complete file-list
      if [[ $tool == *-project ]]; then
        EXTRA_PARAM="symbol-table-defs --file_list_root=/ --file_list_path"
        # a <(echo $single_file) does not work, so use actual file.
        echo ${single_file} > ${PROJECT_FILE_LIST}
        file_param="${PROJECT_FILE_LIST}"
       elif [[ $tool == *-extractor ]]; then
        EXTRA_PARAM="--file_list_root=/ --file_list_path"
        # a <(echo $single_file) does not work, so use actual file.
        echo ${single_file} > ${PROJECT_FILE_LIST}
        file_param="${PROJECT_FILE_LIST}"
       elif [[ $tool == *-lint ]]; then
        EXTRA_PARAM="--lint_fatal=false"
	file_param=${single_file}
       else
        EXTRA_PARAM=""
        file_param=${single_file}
      fi

      ${BINARY_BASE_DIR}/${tool} ${EXTRA_PARAM} ${file_param} > ${TOOL_OUT} 2>&1
      local EXIT_CODE=$?

      # Even though we don't fail globally, let's at least count how many times
      # our tools exit with non-zero. Long term, we'd like to have them succeed
      # on all files
      if [ $EXIT_CODE -ne 0 ]; then
        non_zero_exit_code=$[non_zero_exit_code + 1]
      fi

      # A regular error exit code we accept as normal operation of the tool if
      # it encountered a syntax error. Here, we are only interested in not
      # receiving a signal such as an abort or segmentation fault.
      # So we check for 126, 127 (command not found or executable)
      # and >= 128 (128 + signal-number)
      # https://www.gnu.org/software/bash/manual/html_node/Exit-Status.html
      if [ $EXIT_CODE -ge 126 ]; then
        if [[ $tool == *-project ]]; then
          file_param="<(echo $single_file)"   # make easy to reproduce
        fi
        echo "${TERM_RED}${BINARY_BASE_DIR}/${tool} ${EXTRA_PARAM} ${file_param} ${TERM_RESET}"
        waive_file_key="${short_tool_name}:${single_file}"
        waive_project_key="${short_tool_name}:${PROJECT_NAME}"
        if [[ -v KnownIssue[${waive_file_key}] ]]; then
          bug_number=${KnownIssue[${waive_file_key}]}
          echo "  --> Known issue: 🐞 https://github.com/chipsalliance/verible/issues/$bug_number"
          unset KnownIssue[${waive_file_key}]
        elif [[ -v KnownProjectToolIssue[${waive_project_key}] ]]; then
          echo "  --> Known issue 🐞🐞 possibly one of ${KnownProjectToolIssue[${waive_project_key}]}"
        else
          # This is an so far unknown issue
          echo "::error:: 😱 ${single_file}: crash exit code $EXIT_CODE for $tool"
          echo "Input File URL: ${GIT_URL}/blob/master/$(echo $single_file | cut -d/ -f6-)"
          head -15 ${TOOL_OUT}   # Might be useful in this case
          result=$((${result} + 1))
        fi
      fi
    done < ${FILELIST}
    echo "  -> Non zero exit code ${non_zero_exit_code}/${NUM_FILES}"
  done  # for tool

  rm -f ${PROJECT_FILE_LIST} ${TOOL_OUT}
  return ${result}
}

mkdir -p $BASE_TEST_DIR
trap 'rm -rf -- "$BASE_TEST_DIR"' EXIT

status_sum=0

for git_project in ${TEST_GIT_PROJECTS} ; do
  PROJECT_NAME=$(basename $git_project)
  PROJECT_DIR=${BASE_TEST_DIR}/${PROJECT_NAME}
  git clone -q ${git_project} ${PROJECT_DIR} 2>/dev/null

  # Just collect everything that looks like Verilog/SystemVerilog
  FILELIST=${PROJECT_DIR}/verible.filelist
  find ${PROJECT_DIR} -name "*.sv" -o -name "*.svh" -o -name "*.v" | sort > ${FILELIST}

  run_smoke_test ${PROJECT_NAME} ${FILELIST} ${git_project}
  status_sum=$((${status_sum} + $?))
  echo
done

echo "::endgroup::"

echo "There were a total of ${status_sum} new, undocumented issues."

# Let's see if there are any issues that are fixed in the meantime.
if [ ${#KnownIssue[@]} -ne 0 ]; then
  echo "::warning ::There are ${#KnownIssue[@]} tool/file combinations, that no longer fail"
  declare -A DistinctIssues
  for key in "${!KnownIssue[@]}"; do
    echo " 🎉 ${key}"
    DistinctIssues[${KnownIssue[${key}]}]=1
  done
  echo
  echo "::notice ::These were referencing the following issues. Maybe they are fixed now ?"
  for issue_id in "${!DistinctIssues[@]}"; do
    echo " 🐞 https://github.com/chipsalliance/verible/issues/$issue_id"
  done
  echo
fi

exit ${status_sum}
