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

if [ -z "${BAZEL_VERSION}" ]; then
        echo "Set \$BAZEL_VERSION"
        exit 1
fi

if [[ "${ARCH}" != "arm64" ]]; then
    ARCH="x86_64"
fi

BAZEL_EXEC="/usr/bin/bazel"
wget --no-verbose "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-linux-"${ARCH} -O ${BAZEL_EXEC}
chmod +x ${BAZEL_EXEC}
bazel --version
