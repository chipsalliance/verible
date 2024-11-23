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
$0 binary1 binary2 paths... > diff.diff

Compares the results of two formatter builds.

Runs from current directory, which should be some common ancestor of <paths>.
Binary locations should be absolute.
File copies and intermediate results are written to a temporary directory.
Prints results to stdout (so best to redirect to file, like > diff.diff).
The output is in unified-diff (-u) format.
EOF
}

test "$#" -ge 3 || { usage; exit 1; }

binary1="$(readlink -f $1)"
binary2="$(readlink -f $2)"
shift
shift
# remaining positional arguments are paths...

date=$(date +%Y%m%d-%H%M%S)
globs=("*.sv" "*.svh" "*.v" "*.vh")
temproot=${TMPDIR:=/tmp}
tempdir="$temproot/$(basename $0).tmp/run-$date"

# Restrict paths, so it is (a little safer) to concatenate them
# relative to a temporary dir.
for path
do [[ "$path" != .* ]] ||
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

# snapshot unformatted copies
rsync --files-from="$tempdir/filelist" . "$tempdir/orig/"
cp -r "$tempdir/orig" "$tempdir/formatted-1"
cp -r "$tempdir/orig" "$tempdir/formatted-2"

# Format all files in-place (ignore these exit statuses)
(cd "$tempdir/formatted-1" &&
  find . -type f | xargs -n 1 -P 8 "$binary1" --inplace || :
) &
(cd "$tempdir/formatted-2" &&
  find . -type f | xargs -n 1 -P 8 "$binary2" --inplace || :
) &

wait

# note version information
echo "unformatted" > "$tempdir/orig/VERSION"
"$binary1" --version > "$tempdir/formatted-1/VERSION"
"$binary2" --version > "$tempdir/formatted-2/VERSION"

(cd "$tempdir" &&
  diff -u -r orig formatted-1 > common.diff || :
  diff -u -r formatted-1 formatted-2 > diff.diff || :
)

# print to stdout
cat "$tempdir/diff.diff"

