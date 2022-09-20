#!/usr/bin/env bash
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

set -u
set -o pipefail

TMPDIR="${TMPDIR:-/tmp}"

readonly COMPDB_FILE=bazel/compilation_db_seed_targets.bzl
readonly TMP_COMPDB_FILE=${TMPDIR}/compilation_db_seed_targets.bzl.$$
readonly TMP_OUT=${TMPDIR}/bazel-run.out.$$

readonly OUTPUT_BASE=$(bazel info output_base)

EXIT_CODE=0

trap 'rm -f -- "${TMP_COMPDB_FILE}" "${TMP_OUT}"' EXIT

# Create a list of seed targets to build a compilation database.
# Based on these, the compilation database then will also find all the
# libraries we depend on.
#
# We need to create this list _outside_ the actual compilation_databse() build,
# as we can't use the analysis result of a bazel query inside bazel.
#
# So instead, create the list of all targets here independently, and write
# into a file that then can be included in bazel.
#
# TODO: this should be cc_binary|cc_test, but won't work yet
#   in case bazel version >= 5 is used due to
#   https://github.com/bazelbuild/bazel/issues/14294
bazel query 'visible("//:compdb", kind("cc_binary", "//..."))' 2>/dev/null \
  | LC_ALL=C sort -f | uniq \
  | awk '
BEGIN {
  rule_count=0;
  print("# Generated file; do not edit.");
  print("# Update with .github/bin/make-compilation-db.sh script.");
  print("COMPDB_SEED_TARGETS = [");
}
{ printf("    \"%s\",\n", $1); ++rule_count; }
END {
  printf("]  # %d targets.\n", rule_count);
}
' > ${TMP_COMPDB_FILE}

if [ $? -ne 0 ]; then
  echo "Can't do blaze query"
  exit 1
fi

# Only update needed if we see a difference, e.g. newly added targets.
diff -u ${COMPDB_FILE} ${TMP_COMPDB_FILE}
if [ $? -ne 0 ]; then
  echo
  echo "Changed file ${COMPDB_FILE}"
  bazel shutdown  # Make sure it will properly reload after we change file.
  mv ${TMP_COMPDB_FILE} ${COMPDB_FILE}

  echo "--------------------------------------------"
  echo "Set of available targets changed. If you see this breaking the CI, run "
  echo "  .github/bin/make-compilation-db.sh"
  echo "... and add the changed ${COMPDB_FILE} to the PR"
  echo "--------------------------------------------"
  EXIT_CODE=1
fi

# Build the compilation database baseline with placeholders for exec-root
bazel build :compdb > ${TMP_OUT} 2>&1
if [ $? -ne 0 ]; then
  # Only output something if we had trouble building the compdb. Otherwise,
  # follow no-news-are-good-news principle.
  echo "Trouble building the :compdb"
  cat ${TMP_OUT}
  exit 1
fi

# Fix up the __OUTPUT_BASE__ to the path used by bazel and put the resulting
# db with the expected name in the root directory of the project.
cat bazel-bin/compile_commands.json \
  | sed "s|__OUTPUT_BASE__|$OUTPUT_BASE|g" \
  | sed 's/-fno-canonical-system-headers//g' \
        > compile_commands.json

# Fail CI if files changed.
exit ${EXIT_CODE}
