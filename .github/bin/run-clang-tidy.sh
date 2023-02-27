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

# Script to run clang-tidy on files in a bazel project while caching the
# results as clang-tidy can be pretty slow. The clang-tidy output messages
# are content-addressed in a hash(cc-file-content) cache file.

set -u

# If enviornment variable CACHE_DIR is set, uses that as base to assemble
# a path for the cache. Otherwise ~/.cache, ${TMPDIR}, or /tmp whatever first.

readonly PROJECT_NAME=verible           # To assemble name inside ${CACHE_DIR}
readonly EXIT_CODE_ON_WARNINGS=1        # Should fail on warnings found ?
readonly CLANG_CONFIG_FILE=.clang-tidy  # config file used

if [ ! -f WORKSPACE ]; then
  echo "This script needs to be invoked in the root of the bazel project"
  exit 1
fi

# The caller can supply additional clang-tidy flags that are passed as-is
# Example:
#   run-clang-tidy.sh --checks="-*,modernize-use-equals-default" --fix
readonly ADDITIONAL_TIDY_OPTIONS=("$@")
readonly PARALLEL_COUNT="$(nproc)"

# clang-tidy binary name can be provided in CLANG_TIDY environment variable.
# If not set, try to auto-determine. Prefer clang-tidy-11 in that
# case as it is the last that supports google-runtime-references.
CLANG_TIDY="${CLANG_TIDY:-empty_env}"
if [ "${CLANG_TIDY}" == "empty_env" ]; then
  # fallback search to what is found in $PATH, with version priority
  CLANG_TIDY=clang-tidy
  for version in 11 12 13 14 15; do
    if command -v "clang-tidy-${version}"; then
      CLANG_TIDY="clang-tidy-${version}"
      break
    fi
  done
fi

hash "${CLANG_TIDY}" || exit 2  # make sure it is installed.

EXIT_CODE=0

# Find best place to put the cache.
TMPDIR="${TMPDIR:-/tmp}"
CACHE_DIR="${CACHE_DIR:-empty_env}"
if [ "${CACHE_DIR}" == "empty_env" ]; then
  if [ -d "${HOME}/.cache" ]; then
    CACHE_DIR="${HOME}/.cache/clang-tidy"
    mkdir -p "${CACHE_DIR}"
  else
    CACHE_DIR="${TMPDIR}"
  fi
fi

# Assemble unique name depending on the configuration. This allows to play with
# various clang-tidy versions and have outputs cached in separate directories.
readonly CLANG_SHORT_VERSION="$(${CLANG_TIDY} --version | sed 's/.*version \([0-9]\+\).*/\1/p;d')"
readonly CONFIG_NAME="v${CLANG_SHORT_VERSION}_$((${CLANG_TIDY} --version; cat ${CLANG_CONFIG_FILE} WORKSPACE; echo ${ADDITIONAL_TIDY_OPTIONS[@]}) | md5sum | cut -b1-8)"

readonly BASE_DIR="${CACHE_DIR}/${PROJECT_NAME}-clang-tidy-${CONFIG_NAME}"
readonly BASE_DIR_TMP="${BASE_DIR}/tmp"
readonly CONTENT_DIR="${BASE_DIR}/contents"
mkdir -p "${BASE_DIR}" "${BASE_DIR_TMP}" "${CONTENT_DIR}"
echo "Cache directory: ${BASE_DIR}/"

readonly CLANG_TIDY_CONFIG_MSG="${BASE_DIR_TMP}/config.msg"
readonly INCLUDE_REPLACE="${BASE_DIR_TMP}/include-replace.expression"
readonly FILES_OF_INTEREST="${BASE_DIR_TMP}/files.all"
readonly HASHES_OF_INTEREST="${BASE_DIR_TMP}/hashes.interest"
readonly FILE_HASHES_TO_PROCESS="${BASE_DIR_TMP}/files-hash.process"
readonly CLANG_TIDY_HELPER="${BASE_DIR_TMP}/run-clang-tidy.sh"
trap 'rm -rf -- "${BASE_DIR_TMP}"' EXIT

readonly TIDY_OUT="${BASE_DIR}/tidy.out"
readonly SUMMARY_OUT="${BASE_DIR}/tidy.out.summary"
readonly CURRENT_TIDY_OUT="${PROJECT_NAME}-clang-tidy-current.out"

# Explicitly test the configuration, otherwise clang-tidy will silently
# fall back to some minimal default config.

# A particular annoying thing: if there is a comment in the configuration,
# checks are ignored. Silently.
awk '
BEGIN      { in_rules = 0; }
/^Checks:/ { in_rules = 1; }
/#/        { if (in_rules)  { printf("Comment found %s\n", $0); exit(1); } }
END        { if (!in_rules) { printf("Never seen Checks: >\n"); exit(2); } }
'  < "${CLANG_CONFIG_FILE}"

if [ $? -ne 0 ]; then
  echo "::error:: config not valid: clang-tidy can't deal with comments."
  exit 1
fi

# If there is an issue with the configuration, clang tidy will
# not exit with a non-zero exit code (at least clang-tidy-11)
# But at least there will be an error message to stderr.
"${CLANG_TIDY}" --config="$(cat ${CLANG_CONFIG_FILE})" --dump-config \
  > /dev/null 2> "${CLANG_TIDY_CONFIG_MSG}"
if [ -s "${CLANG_TIDY_CONFIG_MSG}" ]; then
  cat "${CLANG_TIDY_CONFIG_MSG}"
  exit 1
fi

: > "${HASHES_OF_INTEREST}"      # truncate if exists; we'll re-create below
: > "${FILE_HASHES_TO_PROCESS}"

# All the files we want to run clang-tidy on: *.cc and *.h
find . -name "*.cc" -or -name "*.h" \
  | grep -v "verilog/tools/ls/vscode" \
  | sed 's|^\./||' > "${FILES_OF_INTEREST}"

# Changing a header should also process all files depending on it.
# We can't do full C-preprocessing as we don't have all -I available, so
# we simply replace references to headers with hashes of their content.
# If content of these files change, the hashes of the includers will change.
#   #include "foo/bar/baz.h" -> #include "a46b89dd7a7f8f108b836b50762fb6e1"
# Ideally loop until no changes in hashes, but one level good enough.

# Create a sed expression that replaces header filenames with content hash.
for f in $(cat ${FILES_OF_INTEREST}); do
  if [ "${f##*.}" == "h" ]; then     # only headers expected to be relevant
    c_hash="$(cat $f | md5sum | awk '{print $1}')"
    echo "s|$f|$c_hash|g" | sed 's|\.|\\.|g'
  fi
done > "${INCLUDE_REPLACE}"

# Create a content hash of each file, first changing include filenames with
# their content-hash as described above.
# Make a list of all files we don't have hashes for yet.
for f in $(cat ${FILES_OF_INTEREST}); do
  c_hash="$(sed -f "${INCLUDE_REPLACE}" < $f | md5sum | awk '{print $1}')"
  echo "${c_hash}" >> "${HASHES_OF_INTEREST}"
  if [ ! -r "${CONTENT_DIR}/${c_hash}" ]; then  # don't have, need to create
    echo "${c_hash} ${f}" >> "${FILE_HASHES_TO_PROCESS}"
  else
    touch "${CONTENT_DIR}/${c_hash}"  # keep current to prevent garbage collect
  fi
done

echo "$(wc -l < ${HASHES_OF_INTEREST}) files of interest."

if [ -s "${FILE_HASHES_TO_PROCESS}" ]; then
  echo "::group::$(wc -l < ${FILE_HASHES_TO_PROCESS}) files to process"
  cat "${FILE_HASHES_TO_PROCESS}"
  echo "::endgroup::"

  # A compilation database is needed to run clang tidy.
  echo ::group::Build compilation database
  $(dirname $0)/make-compilation-db.sh
  echo "::endgroup::"

  # The files might be reported with various absolute prefixes that
  # we remove to canonicalize and report as relative paths from project
  # root.
  readonly EXEC_ROOT="$(bazel info execution_root)"
  readonly CURRENT_DIR="$(pwd)"
  readonly CANONICALIZE_SOURCE_PATH_REGEX="^\(${EXEC_ROOT}\|${CURRENT_DIR}\|\.\)/"

  # Helper script gets "<hash-value> <filename>" as single argument.
  # Run clang-tidy on <filename> and store result in ${CONTENT_DIR}/<hash>
  cat > "${CLANG_TIDY_HELPER}" <<EOF
read OUTPUT_FILE INPUT_FILE <<< \$1
${CLANG_TIDY} --quiet --config="$(cat ${CLANG_CONFIG_FILE})" \
  ${ADDITIONAL_TIDY_OPTIONS[@]@Q} "\${INPUT_FILE}" 2>/dev/null \
  | sed -e "s@${CANONICALIZE_SOURCE_PATH_REGEX}@@g" \
  > "${CONTENT_DIR}/\${OUTPUT_FILE}.tmp"
mv "${CONTENT_DIR}/\${OUTPUT_FILE}".tmp "${CONTENT_DIR}/\${OUTPUT_FILE}"
EOF

  # Call the helper shell script in parallel that gets the file/hash tuple
  # as parameter.
  echo "Processing in ${PARALLEL_COUNT} parallel invocations of ${CLANG_TIDY}"
  cat "${FILE_HASHES_TO_PROCESS}" | \
    xargs -P"${PARALLEL_COUNT}" -d'\n' -n1 -- "${SHELL}" "${CLANG_TIDY_HELPER}"
fi

# Assemble the outputs of all files of interest into one clang tidy output
while read -r content_hash ; do
  cat "${CONTENT_DIR}/${content_hash}"
done < "${HASHES_OF_INTEREST}" > "${TIDY_OUT}"

ln -sf "${TIDY_OUT}" "${CURRENT_TIDY_OUT}"  # Convenience location

if [ -s "${TIDY_OUT}" ]; then
  EXIT_CODE="${EXIT_CODE_ON_WARNINGS}"
  echo "::warning::There were clang-tidy warnings. Please fix."
  echo "::group::clang-tidy detailed output"
  cat "${TIDY_OUT}"
  echo "Find this output in ${CURRENT_TIDY_OUT}"
  echo "::endgroup::"

  echo "Summary (in ${SUMMARY_OUT})"
  sed -e 's|\(.*\)\(\[[a-zA-Z.-]*\]$\)|\2|p;d' < "${TIDY_OUT}" \
    | sort | uniq -c | sort -rn > "${SUMMARY_OUT}"
  cat "${SUMMARY_OUT}"
else
  echo "No clang-tidy complaints.😎"
fi

find "${CONTENT_DIR}" -type f -mtime +30 -delete  # garbage collect not used

exit "${EXIT_CODE}"
