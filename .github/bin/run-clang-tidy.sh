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

if [ ! -f WORKSPACE ]; then
  echo "This script needs to be invoked in the root of the bazel project"
  exit 1
fi

# The caller can supply additional clang-tidy flags that are passed
# to it as-is.
# Example:
# .github/bin/run-clang-tidy.sh --checks="-*,modernize-use-equals-default" --fix
readonly ADDITIONAL_TIDY_OPTIONS=("$@")

readonly CLANG_CONFIG_FILE=.clang-tidy

TMPDIR="${TMPDIR:-/tmp}"
readonly NAME_PREFIX=verible-clang-tidy  # Make files easy to tab-complete-find
readonly CLANG_TIDY_CONFIG_MSG=${TMPDIR}/${NAME_PREFIX}-config.msg

readonly PARALLEL_COUNT=$(nproc)
readonly FILES_PER_INVOCATION=1

readonly CLANG_TIDY_SEEN_CACHE=${TMPDIR}/${NAME_PREFIX}-hashes.cache
touch ${CLANG_TIDY_SEEN_CACHE}  # Just in case it is not there yet

readonly FILES_TO_PROCESS=${TMPDIR}/${NAME_PREFIX}-files.list
readonly TIDY_OUT=${TMPDIR}/${NAME_PREFIX}.out

# Use clang-tidy-11 if available as it still checks for
# google-runtime-references, non-const references - which is the
# preferred style in this project.
# If not, increase version until we find some that is available.
# (from old to new, as newer ones might have warnings not yet
# anticipated in the clang-tidy configuration).
CLANG_TIDY=clang-tidy
for version in 11 12 13 14 ; do
  if command -v clang-tidy-${version}; then
    CLANG_TIDY=clang-tidy-${version}
    break
  fi
done
hash ${CLANG_TIDY} || exit 2  # make sure it is installed.

# Explicitly test the configuration, otherwise clang-tidy will silently
# fall back to some minimal default config.

# A particular annoying thing: if there is a comment in the configuration,
# things break. Silently.
awk '
BEGIN      { in_rules = 0; }
/^Checks:/ { in_rules = 1; }
/#/        { if (in_rules)  { printf("Comment found %s\n", $0); exit(1); } }
END        { if (!in_rules) { printf("Never seen Checks: >\n"); exit(2); } }' \
      < ${CLANG_CONFIG_FILE}

if [ $? -ne 0 ]; then
  echo "::error:: config not valid: clang-tidy can't deal with comments."
  exit 1
fi

# If there is an issue with the configuration, clang tidy will
# not exit with a non-zero exit code (at least clang-tidy-11)
# But at least there will be an error message to stderr.
${CLANG_TIDY} --config="$(cat ${CLANG_CONFIG_FILE})" --dump-config \
  > /dev/null 2> ${CLANG_TIDY_CONFIG_MSG}
if [ -s ${CLANG_TIDY_CONFIG_MSG} ]; then
  cat ${CLANG_TIDY_CONFIG_MSG}
  exit 1
fi

echo ::group::Build compilation database

time $(dirname $0)/make-compilation-db.sh

# The files might be reported with various absolute prefixes that
# we remove to canonicalize and report as relative paths from project
# root.
readonly EXEC_ROOT="$(bazel info execution_root)"
readonly CURRENT_DIR="$(pwd)"
readonly CANONICALIZE_SOURCE_PATH_REGEX="^\(${EXEC_ROOT}\|${CURRENT_DIR}\|\.\)/"

# Exclude kythe for now, as it is somehwat noisy and should be
# addressed separately.
#
# We create a hash of each file content to only have to look at new files.
# (TODO: could the tidy result be different if an include content changes ?
#  Then we have to do g++ -E (using compilation database knowing about -I etc.)
#  or combine hashes of all mentioned #includes that are also in our list)
# Make need to re-run file dependent on clang-tidy configuration, WORKSPACE
# and of course file content.
for f in $(find . -name "*.cc" -or -name "*.h" \
             | grep -v "verilog/tools/kythe" \
             | grep -v "verilog/tools/ls/vscode" \
           )
do
  (${CLANG_TIDY} --version; cat ${CLANG_CONFIG_FILE} WORKSPACE $f) \
    | md5sum | sed -e "s|-|$f|g"
done | sort > "${CLANG_TIDY_SEEN_CACHE}".new

# Only the files with different hashes are the ones we want to process.
join -v2 "${CLANG_TIDY_SEEN_CACHE}" "${CLANG_TIDY_SEEN_CACHE}".new \
  | awk '{print $2}' | sort > "${FILES_TO_PROCESS}"

echo "::group::$(wc -l < ${FILES_TO_PROCESS}) files to process"
cat "${FILES_TO_PROCESS}"
echo "::endgroup::"

EXIT_CODE=0
if [ -s ${FILES_TO_PROCESS} ]; then
  echo "::group::Run ${PARALLEL_COUNT} parallel invocations of ${CLANG_TIDY} in chunks of ${FILES_PER_INVOCATION} files."

  cat ${FILES_TO_PROCESS} \
    | xargs -P${PARALLEL_COUNT} -n ${FILES_PER_INVOCATION} -- \
      ${CLANG_TIDY} --config="$(cat ${CLANG_CONFIG_FILE})" --quiet "${ADDITIONAL_TIDY_OPTIONS[@]}" 2>/dev/null \
    | sed -e "s@${CANONICALIZE_SOURCE_PATH_REGEX}@@g" \
    > ${TIDY_OUT}.tmp

  mv ${TIDY_OUT}.tmp ${TIDY_OUT}
  cat ${TIDY_OUT}
  echo "::endgroup::"

  echo ::group::Summary
  sed -e 's|\(.*\)\(\[[a-zA-Z.-]*\]$\)|\2|p;d' < ${TIDY_OUT} | sort | uniq -c | sort -rn
  echo "::endgroup::"
else
  echo "Skipping clang-tidy run: nothing to do"
fi


if [ -s "${TIDY_OUT}" ]; then
  EXIT_CODE=1
  echo "::error::There were clang-tidy warnings. Please fix"
  echo "You find the raw output in ${TIDY_OUT}"
  # Assemble a sed expression that matches all files that were mentioned
  # in the clang-tidy output.
  # Use that to filter the file list cache to leave out files with failures,
  # but keep the ones that had no issues.
  # That way, only files that had issues will have to be processed in
  # subsequent clang-tidy runs.

  # Extract filenames from error messages in clang-tidy output
  sed 's/^\([a-zA-Z0-9_/.-]\+\):[0-9].*/\1/p;d' "${TIDY_OUT}" \
    | sort | uniq > "${CLANG_TIDY_SEEN_CACHE}".files_with_issues

  # Escape regex special characters in paths, then convert to delete sed expr
  sed 's|\([/.-]\)|\\\1|g' "${CLANG_TIDY_SEEN_CACHE}".files_with_issues \
    | sed 's|^.*$|/\0/d|' > "${CLANG_TIDY_SEEN_CACHE}".exclude_expression

  # Exclude all lines in cache representing files that need reprocessing
  sed -f "${CLANG_TIDY_SEEN_CACHE}".exclude_expression \
    < "${CLANG_TIDY_SEEN_CACHE}".new > "${CLANG_TIDY_SEEN_CACHE}"
else
  # No complaints. We can keep the enire list now as baseline for next time.
  cp "${CLANG_TIDY_SEEN_CACHE}".new "${CLANG_TIDY_SEEN_CACHE}"
  echo "No clang-tidy complaints.😎"
fi

echo "To reduce the work on next invocation, keep ${CLANG_TIDY_SEEN_CACHE}"

exit ${EXIT_CODE}
