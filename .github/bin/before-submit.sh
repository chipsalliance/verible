#!/usr/bin/env bash
#
# A wrapper for all the things to do before a submit.

PROJECT_ROOT=$(dirname $0)/../../
cd ${PROJECT_ROOT}

RUN_CLANG_TIDY=1
while [ $# -ne 0 ]; do
  case $1 in
    --skip-clang-tidy) RUN_CLANG_TIDY=0;;
    *)
      echo "Unknown option $1"
      exit 1
      ;;
  esac
  shift 1
done

if [ -t 1 ]; then
  BOLD=$'\033[7m'
  RED=$'\033[1;30;41m'
  NORM=$'\033[0m'
else
  BOLD=""
  NORM=""
fi

FINAL_EXIT_CODE=0
function check_exit() {
  exit_code=$?
  if [ ${exit_code} -ne 0 ]; then
    FINAL_EXIT_CODE=$((FINAL_EXIT_CODE + exit_code))
    echo "${RED} ^^ Got exit code ${exit_code} ^^ ${NORM}"
  fi
}

echo "${BOLD}-- Build bant if not already (comp-db, build-cleaner) --${NORM}"
BANT=$($(dirname $0)/get-bant-path.sh)

# Compilation DB is needed for clang-tidy, but also
# makes sure all external dependencies have been fetched so that
# bant build cleaner can do a good job.
echo "${BOLD}-- Refresh compilation db --${NORM}"
.github/bin/make-compilation-db.sh
check_exit

echo "${BOLD}-- Run build cleaner --${NORM}"
. <(${BANT} dwyu ...)

echo "${BOLD}-- Run all tests --${NORM}"
bazel test -c opt --test_summary=terse ...
check_exit

for cpp_standard in c++20 c++23 ; do
  echo "${BOLD}-- Run tests wtih -std=${cpp_standard} --${NORM}"
  bazel test -c opt --cxxopt=-std=${cpp_standard} --test_summary=terse ...
  check_exit
done

if [ "${RUN_CLANG_TIDY}" -eq 1 ]; then
  echo "${BOLD}-- Running clang-tidy and cache results --${NORM}"
  echo "This will take a while if run the first time and no cache has"
  echo "been created yet. Can't wait ? Skip with "
  echo "  $0 --skip-clang-tidy"
  .github/bin/run-clang-tidy-cached.cc
  check_exit
fi

echo "${BOLD}-- Format code and BUILD files --${NORM}"
.github/bin/run-format.sh

echo "${BOLD}-- Check for other potential problems --${NORM}"
.github/bin/check-potential-problems.sh
check_exit

echo "Done with before-submit checks."

if [ ${FINAL_EXIT_CODE} -ne 0 ]; then
  echo "${RED}Some failures. See in log above what's going on${NORM}"
fi

echo "Exit ${FINAL_EXIT_CODE}"
exit ${FINAL_EXIT_CODE}
