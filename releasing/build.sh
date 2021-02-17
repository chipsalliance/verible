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

set -e

# Install gflags2man

wget --no-verbose https://repo.anaconda.com/miniconda/Miniconda2-latest-Linux-x86_64.sh; \
  chmod a+x Miniconda2-latest-Linux-x86_64.sh; \
  ./Miniconda2-latest-Linux-x86_64.sh -p /usr/local -b -f; \
  conda --version
which pip
/usr/local/bin/pip install python-gflags
chmod a+x /usr/local/bin/gflags2man.py
ln -s /usr/local/bin/gflags2man.py /usr/bin/gflags2man
gflags2man

# Build Verible

which bazel
bazel --version

bazel build --workspace_status_command=bazel/build-version.py $BAZEL_OPTS //...

echo "REPO_SLUG: $REPO_SLUG"
echo "GIT_DATE: $GIT_DATE"
echo "GIT_HASH: $GIT_HASH"
echo "GIT_VERSION: $GIT_VERSION"

./.github/bin/github-pages-setup.sh
./.github/bin/github-releases-setup.sh /wrk/out
