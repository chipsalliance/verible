#!/usr/bin/env bash
# Copyright 2024-2025 The Verible Authors.
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
set -e

BANT=$($(dirname $0)/get-bant-path.sh)

if "${BANT}" -q dwyu ... ; then
  echo "Dependencies ok." >&2
else
  cat >&2 <<EOF

Build dependency issues found, the following one-liner will fix it. Amend PR.

source <(.github/bin/run-build-cleaner.sh)
EOF
  exit 1
fi
