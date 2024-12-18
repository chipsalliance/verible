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


# Generate a directory with html coverage. This prepares it to be uploaded
# to a location to serve or to inspect locally.
#
# NOTE: The CI uses codecov, but this script is useful locally.
#   In the actions, we upload the coverage to codecov.io currently, so
#   this HTML generation script is not used in the CI, but is still
#   useful for local coverage checking before sending a pull request.
#   In case we stop using codecov, this is the starting point to generate
#   and upload coverage.
#
# This requires the `lcov` package to be installed.
#
# Precondition, having run bazel coverage with coverage data collection, e.g.
#   MODE=coverage .github/bin/build-and-test.sh
#

PROJECT_ROOT=$(dirname $0)/../../
OUTPUT_DIR=coverage-html
COVERAGE_DATA=bazel-out/_coverage/_coverage_report.dat

cd $PROJECT_ROOT
if [ ! -r "${COVERAGE_DATA}" ]; then
  echo "First, run coverage tests; invoke"
  echo "  MODE=coverage .github/bin/build-and-test.sh"
  echo
  echo ".. or with a specific target"
  echo "  MODE=coverage .github/bin/build-and-test.sh //foo/bar:baz_test"
  exit
fi

genhtml --ignore-errors inconsistent -o ${OUTPUT_DIR} ${COVERAGE_DATA}
echo "Output in $(realpath ${OUTPUT_DIR}/index.html)"
