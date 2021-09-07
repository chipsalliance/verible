#!/bin/bash
# Simple smoke test by running the formatter over some code-base
# making sure it dos not crash

bazel build :install-binaries

IBEX_DIR=/tmp/test/ibex
git clone https://github.com/lowrisc/ibex ${IBEX_DIR}

# Just collect everything, our goal is to test non-crash indpendent on input
FILELIST=${IBEX_DIR}/ibex.filelist
find ${IBEX_DIR} -name "*.sv" > $FILELIST

echo "== Running all tools. Note that any non-crash exit code is ok =="

# verible-verilog-project crashes right now, so disabled (Issue #917)

for tool in syntax/verible-verilog-syntax \
              lint/verible-verilog-lint \
              formatter/verible-verilog-format \
              #project/verible-verilog-project
do
  EXTRA_PARAM=""
  INPUT_FILES="$(cat ${FILELIST})"
  case $tool in
    *-format) EXTRA_PARAM="--inplace" ;;
    *-project)
      INPUT_FILES=""
      EXTRA_PARAM="symbol-table-defs --file_list_root=/ --file_list_path ${FILELIST}" ;;
  esac

  echo -n $tool
  bazel-bin/verilog/tools/$tool ${EXTRA_PARAM} ${INPUT_FILES} >/dev/null 2>&1
  echo -e "\t(exit code $?)"

  # A regular error exit code we accept as normal operation of the tool if it
  # encountered a syntax error. Here, we are only interested in not receiving
  # a signal such as an abort or segmentation fault.
  # So we check for 126, 127 (command not found or executable) and >= 128 (signal)
  # https://www.gnu.org/software/bash/manual/html_node/Exit-Status.html
  EXIT_CODE=$?
  if [ $EXIT_CODE -ge 126 ]; then
    echo "Got exit code $EXIT_CODE for $tool"
    exit 1
  fi
done
