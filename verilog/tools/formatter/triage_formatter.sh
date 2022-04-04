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
$0 binary timelimit paths...

Identifies test cases 'problematic' to the formatter tool.

Runs from current directory, which should be some common ancestor of <paths>.
Formatter binary location can be relative or absolute.
Time limit is forwarded to /usr/bin/timeout.  Recommend '2s'.
Formatter output is not examined, but stderr messages are checked to classify
issues.
EOF
}

test "$#" -ge 3 || { usage; exit 1; }

binary="$(readlink -f $1)"
timelimit="$2"
shift
shift
# remaining positional arguments are paths...

timeout=/usr/bin/timeout
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

# snapshot unformatted copies
rsync --files-from="$tempdir/filelist" . "$tempdir/orig/"

# note version information
"$binary" --version

echo "### Formatting $num_files files one at a time..."
date

cd "$tempdir/orig"
# Format all files (ignore these exit statuses)
set +e
# Operate serially to minimize risk of crashing host machine.
find . -type f | \
while read f
do
  # Silence all diagnostic messages.  They can be reproduced later.
  # --nofailsafe_success: We want a nonzero exit status to signal error conditions.
  "$timeout" --signal=ALRM "$timelimit" \
    "$binary" --nofailsafe_success "$f" > "$f.formatted" 2> "$f.stderr"
  status="$?"
  case "$status" in
    124) echo "timedout: $f" ;;  # 124 is SIGALRM
      0) ;;
      *)
        if grep -q "token partitions failed to complete" "$f.stderr"
        then
          echo "large partition: $f"
          continue
        fi
        if grep -q "syntax error" "$f.stderr"
        then
          echo "rejected input syntax: $f"
          continue
        fi
        if grep -q "Error lex/parsing-ing formatted output" "$f.stderr"
        then
          echo "corrupted output [syntax error]: $f"
          continue
        fi
        if grep -q "Formatted output is lexically different" "$f.stderr"
        then
          echo "corrupted output [lex diff]: $f"
          continue
        fi
        # When you see this, add more case handling here.
        echo "OTHER error: $f"
        ;;
    esac
  done

echo "### Done."
date

