#!/usr/bin/env bash
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

BAZEL_BUILD_OPTIONS="-c opt"

TMPDIR="${TMPDIR:-/tmp}"
readonly BASE_TEST_DIR="${TMPDIR}/test/verible-smoke-test"

# Write log files to this directory
readonly SMOKE_LOGGING_DIR="${SMOKE_LOGGING_DIR:-$BASE_TEST_DIR/error-logs}"

# Make the directory for the logs
if [ ! -d "${SMOKE_LOGGING_DIR}" ]; then
  mkdir -p "${SMOKE_LOGGING_DIR}"
fi

# Some terminal codes to highlight
readonly TERM_RED=$'\033[1;31m'
readonly TERM_BOLD=$'\033[1m'
readonly TERM_RESET=$'\033[0m'

readonly BINARY_BASE_DIR=bazel-bin/verible/verilog/tools

readonly ISSUE_PREFIX="https://github.com/chipsalliance/verible/issues"

# In case the binaries are run with ASAN:
# By default, failing asan binaries exit with exit code 1.
# Let's change it to something that we can distinguish from 'normal operation'
export ASAN_OPTIONS="exitcode=140"

readonly VERIBLE_TOOLS_TO_RUN="syntax/verible-verilog-syntax \
                               lint/verible-verilog-lint \
                               formatter/verible-verilog-format \
                               project/verible-verilog-project \
                               preprocessor/verible-verilog-preprocessor \
                               kythe/verible-verilog-kythe-extractor"

# A few projects that can be fetched from git and represent a good
# cross-section of different styles.
# Always in search for more Open Source github Verilog and SystemVerilog
# projects to add here.
#
# We run all Verible tools on these.
#
# Not included: https://github.com/google/skywater-pdk (the recursive
# checkout is gigantic.)
#
# There are some known issues which are all recorded in the associative
# array below, mapping them to Verible issue tracker numbers.
# TODO(hzeller): there should be a configuration file that contains two
# columns: URL + hash, so that we can fetch a particular known version not
# a moving target.
readonly TEST_GIT_PROJECTS="https://github.com/lowRISC/ibex \
         https://github.com/lowRISC/opentitan \
         https://github.com/chipsalliance/sv-tests \
         https://github.com/chipsalliance/Cores-VeeR-EH2 \
         https://github.com/chipsalliance/caliptra-rtl \
         https://github.com/openhwgroup/cva6 \
         https://github.com/SymbiFlow/uvm \
         https://github.com/taichi-ishitani/tnoc \
         https://github.com/ijor/fx68k \
         https://github.com/jamieiles/80x86 \
         https://github.com/SymbiFlow/XilinxUnisimLibrary \
         https://github.com/black-parrot/black-parrot
         https://github.com/steveicarus/ivtest \
         https://github.com/trivialmips/nontrivial-mips \
         https://github.com/pulp-platform/axi \
         https://github.com/rsd-devel/rsd \
         https://github.com/syntacore/scr1 \
         https://github.com/olofk/serv \
         https://github.com/bespoke-silicon-group/basejump_stl \
         https://github.com/gtaylormb/opl3_fpga"

##
# Some of the files in the projects will have issues.
# We record each of them with the associated bug number in this set so that
# we can locally waive the issue and continue with more files,
# but also have it documented which are affected.
#
# Any new issues that arise should be recorded in the verible bug-tracker to
# be fixed.
# Goal: The following list shall be empty :)
# Format: <tool-name>:<basename of file-name in project>
declare -A KnownIssue

# At the moment, the list is empty

#--- Too many to mention manually, so here we do the 'waive all' approach
# Format: <tool-name>:<project>
declare -A KnownProjectToolIssue
KnownProjectToolIssue[project:caliptra-rtl]=1946

##
# Some tools still not fully process all files and return a non-zero exit
# count.
# This is to record the expected non-zero exits, so that we can report if
# things improve or regress.
# TODO: get these numbers to zero. If zero, the corresponding line can be
# removed.
declare -A ExpectedFailCount

ExpectedFailCount[syntax:ibex]=14
ExpectedFailCount[lint:ibex]=14
ExpectedFailCount[project:ibex]=211
ExpectedFailCount[preprocessor:ibex]=385

ExpectedFailCount[syntax:opentitan]=65
ExpectedFailCount[lint:opentitan]=65
ExpectedFailCount[project:opentitan]=951
ExpectedFailCount[formatter:opentitan]=1
ExpectedFailCount[preprocessor:opentitan]=2560

ExpectedFailCount[syntax:sv-tests]=77
ExpectedFailCount[lint:sv-tests]=76
ExpectedFailCount[project:sv-tests]=187
ExpectedFailCount[preprocessor:sv-tests]=139

ExpectedFailCount[syntax:caliptra-rtl]=26
ExpectedFailCount[lint:caliptra-rtl]=25
ExpectedFailCount[project:caliptra-rtl]=371
ExpectedFailCount[preprocessor:caliptra-rtl]=802

ExpectedFailCount[syntax:Cores-VeeR-EH2]=2
ExpectedFailCount[lint:Cores-VeeR-EH2]=2
ExpectedFailCount[project:Cores-VeeR-EH2]=42
ExpectedFailCount[preprocessor:Cores-VeeR-EH2]=43

ExpectedFailCount[syntax:cva6]=7
ExpectedFailCount[lint:cva6]=7
ExpectedFailCount[project:cva6]=91
ExpectedFailCount[preprocessor:cva6]=141

ExpectedFailCount[syntax:uvm]=1
ExpectedFailCount[lint:uvm]=1
ExpectedFailCount[project:uvm]=43
ExpectedFailCount[preprocessor:uvm]=115

ExpectedFailCount[syntax:tnoc]=3
ExpectedFailCount[lint:tnoc]=3
ExpectedFailCount[project:tnoc]=24
ExpectedFailCount[preprocessor:tnoc]=57

ExpectedFailCount[project:80x86]=2
ExpectedFailCount[preprocessor:80x86]=7

ExpectedFailCount[syntax:XilinxUnisimLibrary]=4
ExpectedFailCount[lint:XilinxUnisimLibrary]=4
ExpectedFailCount[project:XilinxUnisimLibrary]=22
ExpectedFailCount[preprocessor:XilinxUnisimLibrary]=96

ExpectedFailCount[syntax:black-parrot]=154
ExpectedFailCount[lint:black-parrot]=154
ExpectedFailCount[project:black-parrot]=169
ExpectedFailCount[preprocessor:black-parrot]=170

ExpectedFailCount[syntax:ivtest]=166
ExpectedFailCount[lint:ivtest]=166
ExpectedFailCount[project:ivtest]=198
ExpectedFailCount[preprocessor:ivtest]=26

ExpectedFailCount[syntax:nontrivial-mips]=2
ExpectedFailCount[lint:nontrivial-mips]=2
ExpectedFailCount[project:nontrivial-mips]=81
ExpectedFailCount[preprocessor:nontrivial-mips]=78

ExpectedFailCount[project:axi]=78
ExpectedFailCount[preprocessor:axi]=75

ExpectedFailCount[syntax:rsd]=5
ExpectedFailCount[lint:rsd]=5
ExpectedFailCount[project:rsd]=52
ExpectedFailCount[preprocessor:rsd]=49

ExpectedFailCount[project:scr1]=45
ExpectedFailCount[preprocessor:scr1]=46

ExpectedFailCount[project:serv]=1
ExpectedFailCount[preprocessor:serv]=1

ExpectedFailCount[syntax:basejump_stl]=481
ExpectedFailCount[lint:basejump_stl]=481
ExpectedFailCount[project:basejump_stl]=596
ExpectedFailCount[formatter:basejump_stl]=1
ExpectedFailCount[preprocessor:basejump_stl]=632

ExpectedFailCount[syntax:opl3_fpga]=3
ExpectedFailCount[lint:opl3_fpga]=3
ExpectedFailCount[project:opl3_fpga]=5
ExpectedFailCount[preprocessor:opl3_fpga]=4

# Ideally, we expect all tools to process all files with a zero exit code.
# However, that is not always the case, so we document the current
# state, so that we can see regressions.
# Currently, either way will not change the output.
function verify_expected_non_zero_exit_count() {
  local TOOL_SHORT_NAME=$1
  local PROJECT_NAME=$2
  local FILE_COUNT=$3
  local OBSERVED_NONZERO_COUNT=$4

  echo "  -> Non zero exit code ${OBSERVED_NONZERO_COUNT}/${NUM_FILES}"

  expected_count_key="${TOOL_SHORT_NAME}:${PROJECT_NAME}"
  if [[ -v ExpectedFailCount[${expected_count_key}] ]]; then
    expected_count=${ExpectedFailCount[${expected_count_key}]}
    # We allow some 5% deviation from expected values before we complain loudly
    local GRACE_VALUE=$(( ${expected_count} / 20 ))
    if [ ${GRACE_VALUE} -lt 2 ]; then
      GRACE_VALUE=2
    fi
    if [ ${OBSERVED_NONZERO_COUNT} -gt ${expected_count} ] ; then
      local ALLOWED_UP_TO=$((${expected_count} + ${GRACE_VALUE}))
      if [ ${OBSERVED_NONZERO_COUNT} -gt ${ALLOWED_UP_TO} ]; then
        echo "::error:: 😱 ${expected_count_key} Expected failures ${expected_count}, got ${OBSERVED_NONZERO_COUNT}"
        return 1
      else
        echo "::warning:: 😱 ${expected_count_key} Expected failures ${expected_count}, got ${OBSERVED_NONZERO_COUNT} (will be an error once ${GRACE_VALUE} more)"
        return 0  # Still within grace range.
      fi
    elif [ ${OBSERVED_NONZERO_COUNT} -lt ${expected_count} ] ; then
      echo "::notice:: 🎉 Yay, reduced non-zero exit count ${expected_count} -> ${OBSERVED_NONZERO_COUNT}"
      echo "Set ExpectedFailCount[${TOOL_SHORT_NAME}:${PROJECT_NAME}]=${OBSERVED_NONZERO_COUNT}"
    fi
  else
    if [ ${OBSERVED_NONZERO_COUNT} -gt 0 ] ; then
      echo "::error:: ********************* Not mentioned in ExpectedFailCount *******"
      echo "Add  ExpectedFailCount[${TOOL_SHORT_NAME}:${PROJECT_NAME}]=${OBSERVED_NONZERO_COUNT}"
      return 1
    fi
  fi
  return 0
}

# Run smoke test on provided files for project.
# Returns 0 if all tools finished without crashing.
#
# First parameter : project name
# Second parameter: name of file containing a list of {System}Verilog files
function run_smoke_test() {
  local PROJECT_FILE_LIST=${TMPDIR}/filelist.$$.list
  local PROJECT_NAME=$1
  local FILELIST=$2
  local GIT_URL=$3
  local NUM_FILES=$(wc -l < ${FILELIST})
  local result=0

  local PROJECT_LOG_BASE="${SMOKE_LOGGING_DIR}/${PROJECT_NAME}"
  mkdir -p ${PROJECT_LOG_BASE}

  echo "::group::== Running verible on ${TERM_BOLD}${PROJECT_NAME}${TERM_RESET} with ${NUM_FILES} files =="

  for tool in $VERIBLE_TOOLS_TO_RUN ; do
    printf "%-20s %-32s\n" ${PROJECT_NAME} ${tool}
    local short_tool_name=$(dirname ${tool})
    local non_zero_exit_code=0
    local PROJECT_TOOL_LOG_BASE="${PROJECT_LOG_BASE}/${short_tool_name}"
    mkdir -p "${PROJECT_TOOL_LOG_BASE}"

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
      elif [[ $tool == *-preprocessor ]]; then
        EXTRA_PARAM="preprocess"
	      file_param=${single_file}
      else
        EXTRA_PARAM=""
        file_param=${single_file}
      fi
      local FNAME=$(basename ${file_param})  # make hash ?
      local PROJECT_FILE_TOOL_OUT="${PROJECT_TOOL_LOG_BASE}/${FNAME}"

      ${BINARY_BASE_DIR}/${tool} ${EXTRA_PARAM} ${file_param} > ${PROJECT_FILE_TOOL_OUT} 2>&1
      local EXIT_CODE=$?

      if [ $EXIT_CODE -eq 0 ]; then
        rm -f "${PROJECT_FILE_TOOL_OUT}"   # not interested keeping logfiles of successful runs
      else
        # Even though we don't fail globally, let's at least count how many times
        # our tools exit with non-zero. Long term, we'd like to have them succeed
        # on all files
        non_zero_exit_code=$[non_zero_exit_code + 1]

        # The error-log-analyzer.py requires the files in a particular format
        local ANALYZE_DIR="${SMOKE_LOGGING_DIR}/${PROJECT_NAME}-nonzeros"
        mkdir -p "${ANALYZE_DIR}"
        cp "${PROJECT_FILE_TOOL_OUT}" "${ANALYZE_DIR}/${EXIT_CODE}-${FNAME}_${short_tool_name}"
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
        waive_file_key="${short_tool_name}:$(basename ${single_file})"
        waive_project_key="${short_tool_name}:${PROJECT_NAME}"
        if [[ -v KnownIssue[${waive_file_key}] ]]; then
          bug_number=${KnownIssue[${waive_file_key}]}
          echo "  --> Known issue: 🐞 ${ISSUE_PREFIX}/${bug_number}"
          unset KnownIssue[${waive_file_key}]
        elif [[ -v KnownProjectToolIssue[${waive_project_key}] ]]; then
          echo "  --> Known issue 🐞🐞 possibly one of ${ISSUE_PREFIX}/${KnownProjectToolIssue[${waive_project_key}]}"
        else
          # This is an so far unknown issue
          echo "::error:: 😱 ${single_file}: crash exit code $EXIT_CODE for $tool"
          echo "Input File URL: ${GIT_URL}/blob/master/$(echo $single_file | cut -d/ -f6-)"
          head -15 ${PROJECT_FILE_TOOL_OUT}   # Might be useful in this case
          result=$((${result} + 1))
        fi
      fi
    done < ${FILELIST}

    # Let's see if number of non-zero exit codes match what we expect
    verify_expected_non_zero_exit_count ${short_tool_name} ${PROJECT_NAME} ${NUM_FILES} ${non_zero_exit_code}
    result=$((${result} + $?))

  done  # for tool

  rm -f ${PROJECT_FILE_LIST}
  return ${result}
}

mkdir -p "${BASE_TEST_DIR}"
trap 'rm -rf -- "${BASE_TEST_DIR}"' EXIT

status_sum=0

#-- Prepare stuff in parallel
#  - build the binaries
#  - git fetch the projects.

# Build all the installable binaries that we're going to use below.
# TODO: Consider running with
#  * address sanitizer (best result with libc++). CAVE: slow
#  * running with -D_GLIBCXX_DEBUG (also see #1056)
bazel build ${BAZEL_BUILD_OPTIONS} :install-binaries &

# While compiling, run potentially slow network ops
for git_project in ${TEST_GIT_PROJECTS} ; do
  PROJECT_NAME="$(basename $git_project)"
  PROJECT_DIR="${BASE_TEST_DIR}/${PROJECT_NAME}"
  git clone ${git_project} ${PROJECT_DIR} 2>/dev/null &
done

echo "base test dir ${BASE_TEST_DIR}; writing logs to ${SMOKE_LOGGING_DIR}"
echo "Waiting... for compilation and project download finished"
wait

for git_project in ${TEST_GIT_PROJECTS} ; do
  PROJECT_NAME="$(basename $git_project)"
  PROJECT_DIR="${BASE_TEST_DIR}/${PROJECT_NAME}"
  # Already cloned above

  if [ ! -d "${PROJECT_DIR}" ]; then
    echo "Didn't see ${PROJECT_DIR}. Network connection flaky ? Skipping."
    continue
  fi

  # Just collect everything that looks like Verilog/SystemVerilog
  FILELIST="${PROJECT_DIR}/verible.filelist"
  find "${PROJECT_DIR}" -name "*.sv" -o -name "*.svh" -o -name "*.v" | sort > ${FILELIST}

  run_smoke_test "${PROJECT_NAME}" "${FILELIST}" "${git_project}"
  status_sum=$((${status_sum} + $?))
  echo
done

echo "::endgroup::"

echo "There were a total of ${status_sum} new, undocumented issues."

# Let's see if there are any issues that are fixed in the meantime.
if [ "${#KnownIssue[@]}" -ne 0 ]; then
  echo "::warning ::There are ${#KnownIssue[@]} tool/file combinations, that no longer fail"
  declare -A DistinctIssues
  for key in "${!KnownIssue[@]}"; do
    echo " 🎉 ${key}"
    DistinctIssues[${KnownIssue[${key}]}]=1
  done
  echo
  echo "::notice ::These were referencing the following issues. Maybe they are fixed now ?"
  for issue_id in "${!DistinctIssues[@]}"; do
    echo " 🐞 ${ISSUE_PREFIX}/${issue_id}"
  done
  echo
fi

exit ${status_sum}
