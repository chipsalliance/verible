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

readonly BASE_TEST_DIR=/tmp/test/verible-smoke-test

# Build all the installable binaries that we're going to use below.
# Ideally, we'd use --config=asan, but unfortunately, asan exits with exit
# code 1, so we can't distinguish from regular 1 exit :(
#
# (TODO: maybe consider asan and capture the output and grep for
# "AddressSanitizer" string if we get an exit code of 1.)
bazel build :install-binaries
readonly BINARY_BASE_DIR=bazel-bin/verilog/tools

readonly VERIBLE_TOOLS_TO_RUN="syntax/verible-verilog-syntax \
                               lint/verible-verilog-lint \
                               formatter/verible-verilog-format \
                               project/verible-verilog-project"

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
KnownIssue[formatter:$BASE_TEST_DIR/opentitan/hw/ip/otp_ctrl/dv/env/otp_ctrl_if.sv]=1005
KnownIssue[formatter:$BASE_TEST_DIR/opentitan/hw/ip/aes/dv/aes_model_dpi/aes_model_dpi_pkg.sv]=1006
KnownIssue[formatter:$BASE_TEST_DIR/opentitan/hw/top_earlgrey/ip/ast/rtl/aon_osc.sv]=1007
# There is also bug 1008 which only shows up if compiled with asan

#--- ivtest
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/stask_sens_null_arg.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/sv_macro.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/sv_port_default14.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/pr1763333.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/br_gh72a.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/blankport.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/br_gh72b_fail.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/param_times.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/sv_default_port_value1.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/sv_default_port_value3.v]=1010
KnownIssue[formatter:$BASE_TEST_DIR/ivtest/ivltests/pr2202846c.v]=1015
KnownIssue[project:$BASE_TEST_DIR/ivtest/ivltests/wreal.v]=1017

#--- Basejump
# These mostly crash for all the same reason except the first.
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_cache/bsg_cache_pkg.v]=1011
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_comm_link/bsg_source_sync_input.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_comm_link/bsg_source_sync_output.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_comm_link/tests/test_bsg_assembler/test_bsg_assembler.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_comm_link/tests/test_bsg_source_sync/test_bsg_source_sync.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_dataflow/bsg_flow_convert.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_fsb/bsg_fsb_node_trace_replay.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_link/bsg_link_source_sync_downstream.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_link/bsg_link_source_sync_upstream.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_mem/bsg_cam_1r1w_replacement.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_mesosync_io/tests/mesosynctb_gate_level.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_mesosync_io/tests/mesosynctb.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_misc/bsg_idiv_iterative_controller.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_misc/bsg_imul_iterative.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_noc/bsg_wormhole_router_decoder_dor.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_riscv/bsg_hasti/bsg_vscale_hasti_converter.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_riscv/bsg_nasti/bsg_fsb_to_nasti_master_connector.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_riscv/bsg_nasti/bsg_fsb_to_nasti_slave_connector.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_tag/legacy/config_net/src/cfgtaggw.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_tag/legacy/config_net/tests/cfgtaggw_test/cfgtag.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/bsg_test/bsg_trace_replay.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/hard/tsmc_40/bsg_clk_gen/bsg_rp_clk_gen_atomic_delay_tuner.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_dataflow/bsg_channel_tunnel/test_bsg.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_dataflow/bsg_parallel_in_serial_out/bsg_parallel_in_serial_out_tester.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_link/bsg_link_ddr_tester.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_link/bsg_link_sdr/bsg_link_sdr_tester.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_noc/bsg_wormhole_concentrator/bsg_wormhole_concentrator_tester.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_noc/bsg_wormhole_network/bsg_wormhole_network_tester.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_noc/bsg_wormhole_router/bsg_wormhole_router_tester.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_test/bsg_nonsynth_dramsim3/testbench_multi.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_test/bsg_nonsynth_dramsim3/testbench.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_test/bsg_trace_replay/dut.v]=1012
KnownIssue[formatter:$BASE_TEST_DIR/basejump_stl/testing/bsg_test/bsg_trace_replay/test_bench.v]=1012

#--- Blackparrot
KnownIssue[formatter:$BASE_TEST_DIR/black-parrot/bp_me/src/v/network/axi_fifo.sv]=1004
KnownIssue[formatter:$BASE_TEST_DIR/black-parrot/bp_me/src/v/cce/bp_cce_inst_stall.sv]=1004
KnownIssue[formatter:$BASE_TEST_DIR/black-parrot/bp_be/src/include/bp_be_ctl_pkgdef.svh]=1016
KnownIssue[formatter:$BASE_TEST_DIR/black-parrot/bp_common/src/include/bp_common_aviary_pkgdef.svh]=1016
KnownIssue[formatter:$BASE_TEST_DIR/black-parrot/bp_common/src/include/bp_common_bedrock_pkgdef.svh]=1016
KnownIssue[formatter:$BASE_TEST_DIR/black-parrot/bp_common/src/include/bp_common_cache_engine_if.svh]=1016
KnownIssue[formatter:$BASE_TEST_DIR/black-parrot/bp_common/src/include/bp_common_rv64_pkgdef.svh]=1016
KnownIssue[formatter:$BASE_TEST_DIR/black-parrot/bp_me/src/include/bp_me_cce_inst_pkgdef.svh]=1016

#--- Too many to mention manually, so here we do the 'waive all' approach
declare -A KnownProjectToolIssue
KnownProjectToolIssue[project:basejump_stl]="#917 #1002 #1003"
KnownProjectToolIssue[project:ibex]="#917 #1002 #1003"
KnownProjectToolIssue[project:uvm]="#917 #1002 #1003"
KnownProjectToolIssue[project:opentitan]="#917 #1002 #1003"

# Run smoke test on provided files for project.
# Returns 0 if all tools finished without crashing.
#
# First parameter : project name
# Second parameter: name of file containing a list of {System}Verilog files
function run_smoke_test() {
  local PROJECT_FILE_LIST=/tmp/filelist.$$.list
  local PROJECT_NAME=$1
  local FILELIST=$2
  local result=0

  echo "== Running verible on $PROJECT_NAME with $(wc -l < ${FILELIST}) files"

  for tool in $VERIBLE_TOOLS_TO_RUN ; do
    printf "%-20s %-32s\n" ${PROJECT_NAME} ${tool}
    local short_tool_name=$(dirname ${tool})

    while read single_file; do
      if [[ $tool == *-project ]]; then
        EXTRA_PARAM="symbol-table-defs --file_list_root=/ --file_list_path"
        # a <(echo $single_file) does not work, so use actual file.
        echo ${single_file} > ${PROJECT_FILE_LIST}
        file_param="${PROJECT_FILE_LIST}"
      else
        EXTRA_PARAM=""
        file_param=${single_file}
      fi

      ${BINARY_BASE_DIR}/${tool} ${EXTRA_PARAM} ${file_param} >/dev/null 2>&1
      local EXIT_CODE=$?

      # A regular error exit code we accept as normal operation of the tool if
      # it encountered a syntax error. Here, we are only interested in not
      # receiving a signal such as an abort or segmentation fault.
      # So we check for 126, 127 (command not found or executable)
      # and >= 128 (128 + signal-number)
      # https://www.gnu.org/software/bash/manual/html_node/Exit-Status.html
      if [ $EXIT_CODE -ge 126 ]; then
        echo ${BINARY_BASE_DIR}/${tool} ${EXTRA_PARAM} ${file_param}
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
          echo "😱 ${single_file}: crash exit code $EXIT_CODE for $tool"
          result=$((${result} + 1))
        fi
      fi
    done < ${FILELIST}
  done  # for tool

  rm -f ${PROJECT_FILE_LIST}
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

  run_smoke_test ${PROJECT_NAME} ${FILELIST}
  status_sum=$((${status_sum} + $?))
  echo
done

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

echo "There were a total of ${status_sum} new, undocumented issues."
exit ${status_sum}
