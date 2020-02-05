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

echo
echo "========================================"
echo "Git log"
echo "----------------------------------------"
git log -n5 --stat
echo "----------------------------------------"

echo
echo "========================================"
echo "Git fetching tags"
echo "----------------------------------------"
# Don't fail if there are no tags
git fetch --tags || true
echo "----------------------------------------"

echo
echo "========================================"
echo "Git version info"
echo "----------------------------------------"
git log -n1
echo "----------------------------------------"
git describe --tags || true
echo "----------------------------------------"
git describe --tags --always || true
echo "----------------------------------------"
