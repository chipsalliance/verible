#!/bin/bash
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

set -e

CALLED=$_
[[ "${BASH_SOURCE[0]}" != "${0}" ]] && SOURCED=1 || SOURCED=0

SCRIPT_SRC="$(realpath ${BASH_SOURCE[0]})"
SCRIPT_DIR="$(dirname "${SCRIPT_SRC}")"

export PATH="/usr/sbin:/usr/bin:/sbin:/bin"

cd github/$KOKORO_DIR

. $SCRIPT_DIR/../settings.sh
. $SCRIPT_DIR/steps/git.sh
. $SCRIPT_DIR/steps/hostsetup.sh
. $SCRIPT_DIR/steps/hostinfo.sh
. $SCRIPT_DIR/steps/compiler.sh

set +e

echo
echo "---------------------------------------------------------------"
echo " Building Verible"
echo "---------------------------------------------------------------"
set -x
bazel build -c opt --noshow_progress $BAZEL_CXXOPTS //...
RET=$?
set +x

if [[ $RET = 0 ]]; then
  echo
  echo "---------------------------------------------------------------"
  echo " Testing Verible"
  echo "---------------------------------------------------------------"
  set -x
  bazel test --noshow_progress $BAZEL_CXXOPTS //...
  RET=$?
  set +x
else
  echo
  echo "Not testing as building failed."
fi

echo
echo "---------------------------------------------------------------"
echo " Converting test logs"
echo "---------------------------------------------------------------"
. $SCRIPT_DIR/steps/convert-logs.sh

exit $RET
