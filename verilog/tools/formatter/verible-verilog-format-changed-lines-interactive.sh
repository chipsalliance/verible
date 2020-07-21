#!/bin/bash
# verible-verilog-format-changed-lines-interactive.sh.

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

script_name="$(basename $0)"
script_dir="$(dirname $0)"

# Look first for support scripts/binaries installed alongside this script.
# Fall back to searching in current PATH.

# This is the script that drives interactive patching.
interactive_patch_script="$script_dir"/verible-transform-interactive.sh
[[ -x "$interactive_patch_script" ]] || \
  interactive_patch_script="$(which verible-transform-interactive.sh)"

# This is Verible's patch-based utility.
patch_tool="$script_dir"/verible-patch-tool
[[ -x "$patch_tool" ]] || patch_tool="$(which verible-patch-tool)"

# This is Verible's Verilog formatter.
formatter="$script_dir"/verible-verilog-format
[[ -x "$formatter" ]] || formatter="$(which verible-verilog-format)"

function usage() {
  cat <<EOF
$0:
Interactively prompts user to accept/reject formatter tool changes,
based only on touched lines of code in a revision-controlled workspace.

Usage: $script_name [script_options]
Run this from a version-controlled directory.

script options:
  --help, -h : print help and exit
  --rcs TOOL : revision control system
      Supported: p4

EOF
}

function msg()  {
  echo "[$script_name] " "$@"
}

# version control system
rcs=p4

# generic option processing
for opt
do
  # handle: --option arg
  if test -n "$prev_opt"
  then
    eval $prev_opt=\$opt
    prev_opt=
    shift
    continue
  fi
  case "$opt" in
    *=?*) optarg=$(expr "X$opt" : '[^=]*=\(.*\)') ;;
    *=) optarg= ;;
  esac
  case $opt in
    -- ) shift ; break ;; # stop option processing
    --help | -h ) { usage ; exit ;} ;;
    --rcs ) prev_opt=rcs ;;
    --rcs=* ) rcs="$optarg" ;;
    --* | -* ) msg "Unknown option: $opt" ; exit 1 ;;
    # This script expects no positional arguments.
    *) { msg "Unexpected positional argument: $opt" ; usage ; exit 1 ;} ;;
  esac
  shift
done

# TODO(fangism): attempt to infer version control system (p4,svn,git,cvs,hg,...)

# Set commands based on version control system
case "$rcs" in
  p4) touched_files=(p4 whatsout)
      single_file_diff=(p4 diff -d-u "{}") ;;
  *) { msg "Unsupported version control system: $rcs" ; exit 1;} ;;
esac

# File extensions are currently hardcoded to Verilog files.
files=($("${touched_files[@]}" | grep -e '\.sv$' -e '\.v$' -e '\.svh$' -e '\.vh$' ))

interact_command=("$interactive_patch_script" \
  --patch-tool "$patch_tool" \
  --per-file-transform-flags='--lines=$('"${single_file_diff[*]}"' | '"$patch_tool"' changed-lines - | cut -d" " -f2)' \
  -- "$formatter" \
  -- "${files[@]}" )

msg "interact command: ${interact_command[@]}"
exec "${interact_command[@]}"
