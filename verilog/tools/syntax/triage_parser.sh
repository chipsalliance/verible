#!/usr/bin/env bash -e
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

usage() {
  cat <<EOF
$0 binary paths...

Identifies test cases 'problematic' to the parser (verilog_syntax).

Runs from current directory, which should be some common ancestor of <paths>.
Parser binary location can be relative or absolute.
EOF
}

test "$#" -ge 2 || { usage; exit 1; }

binary="$(readlink -f $1)"
shift
# remaining positional arguments are paths...

date=$(date +%Y%m%d-%H%M%S)
globs=("*.sv" "*.svh" "*.v" "*.vh")
temproot=${TMPDIR:=/tmp}
tempdir="$temproot/$(basename $0).tmp/run-$date"

# Restrict paths, so it is (a little safer) to concatenate them
# relative to a temporary dir.
for path
do
  [[ "$path" != .* ]] ||
    { echo "Relative file paths may not start with '.'" ; exit 1; }
done

# echo "Working in $tempdir"
mkdir -p "$tempdir"

# gather filelist
{
for path
do for glob in "${globs[@]}"
  do find "$path" -type f -name "$glob"
  done
done
wait
} | sort > "$tempdir/filelist"

num_files="$(wc -l "$tempdir/filelist" | awk '{print $1;}')"

# create directories where stderr files will go
mkdir -p "$tempdir"/reports
pushd "$tempdir"/reports
cat "$tempdir"/filelist | \
  xargs -n 1 -P 1 dirname | \
  sort -u | xargs mkdir -p
popd

# note version information
"$binary" --version

echo "### Parsing $num_files files one at a time..."
date

# Parse all files (ignore these exit statuses)
set +e
# Operate serially.
cat "$tempdir"/filelist | \
while read f
do
  # Redirect all diagnostic messages.  They can be reproduced later.
  "$binary" "$f" > "$tempdir/reports/$f.stderr" 2>&1
  status="$?"
  case "$status" in
      0) ;;
      *)
        if grep -q "syntax error" "$tempdir/reports/$f.stderr"
        then
          echo "syntax error: $f"
          continue
        fi
        # Likely some crash:
        echo "OTHER error: $f"
        ;;
    esac
  done

echo "### Done."
date

