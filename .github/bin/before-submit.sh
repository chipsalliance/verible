#!/usr/bin/env bash
#
# A wrapper for all the things to do before a submit.

BANT=$($(dirname $0)/get-bant-path.sh)

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
  NORM=$'\033[0m'
else
  BOLD=""
  NORM=""
fi

echo "${BOLD}-- Run build cleaner --${NORM}"
. <(${BANT} dwyu ...)

echo "${BOLD}-- Run all tests --${NORM}"
bazel test -c opt ...

if [ "${RUN_CLANG_TIDY}" -eq 1 ]; then
  # Run clang-tidy; needs a compilation DB first
  .github/bin/make-compilation-db.sh
  echo "${BOLD}-- Running clang-tidy and cache results --${NORM}"
  echo "This will take a while if run the first time and no cache has"
  echo "been created yet. Can't wait ? Skip with "
  echo "  $0 --skip-clang-tidy"
  .github/bin/run-clang-tidy-cached.cc
fi

echo "${BOLD}-- Format code and BUILD files --${NORM}"
.github/bin/run-format.sh

echo "${BOLD}-- Check for potential problems --${NORM}"
.github/bin/check-potential-problems.sh

echo "Done. Check output above for any issues."
