#!/usr/bin/env bash
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

if [ -z "${KYTHE_VERSION}" ]; then
        echo "Set \$KYTHE_VERSION"
        exit 1
fi

set -e

echo "Fetching kythe"

if [[ "$KYTHE_VERSION" == "master" ]]
then
  (
    mkdir kythe-bin
    cd kythe-bin
    # Note: top-of-tree archive does not come with binaries
    # Caution: Building kythe itself will require all of its build dependencies
    # and be extremely compute-intensive.
    wget --no-verbose -O kythe.zip \
      https://github.com/kythe/kythe/archive/master.zip
    unzip kythe.zip
  )
else
  (
    mkdir kythe-bin
    cd kythe-bin
    # Use release, which comes with pre-built binaries
    wget --no-verbose -O kythe.tar.gz \
      "https://github.com/kythe/kythe/releases/download/$KYTHE_VERSION/kythe-$KYTHE_VERSION.tar.gz"
    tar -xzf kythe.tar.gz
  )
fi
