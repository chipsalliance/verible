#!/usr/bin/env bash
# Copyright 2017-2020 The Verible Authors.
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

# Script to expand error messages to show the line where an error occurred.
# Works as a pipe: reads from stdin, writes to stdout.
#
# Input is assumed to contain one diagnostic per line in the following format:
#   FILE:LINE:COL: ...text...
#
# The referenced files must be valid paths from the point of invocation of this
# script.
#
# This script exists as a short term workaround, but the plan is to integrate
# this functionality directly into diagnostic-emitting tools.

function column_marker() {
  # $1 is prefix
  # $2 is the marker
  # $3 is the column position
  printf "$1"'%.s' $(eval "echo {1.."$(($3 - 1))"}");
  echo "$2"
}

while read line
do
  IFS=':' read -r -a array <<< "$line"
  file="${array[0]}"
  lineno="${array[1]}"
  col="${array[2]}"
  echo "$line"
  # Print the referenced line.
  sed -n "$lineno"'p' "$file"
  # Print a column marker.
  column_marker ' ' '^' "$col"
done
