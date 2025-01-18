#!/usr/bin/env bash
# Copyright 2020-2025 The Verible Authors.
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

# Invoke bazel with --workspace_status_command=bazel/build-version.py to get
# this invoked and populate bazel-out/volatile-status.txt

# Get commit timestamp from git if available, otherwise attempt to get
# from COMMIT_TIMESTAMP or GIT_DATE environment variable.
OUTPUT_COMMIT_TIMESTAMP="$(git log -n1 --format=%cd --date=unix 2>/dev/null)"
if [ -z "${OUTPUT_COMMIT_TIMESTAMP}" ]; then
  OUTPUT_COMMIT_TIMESTAMP="${COMMIT_TIMESTAMP}"  # from environment
fi

if [ ! -z "${OUTPUT_COMMIT_TIMESTAMP}" ]; then
  echo "COMMIT_TIMESTAMP ${OUTPUT_COMMIT_TIMESTAMP}"
elif [ ! -z "${GIT_DATE}" ]; then  # legacy environment variable only fallback
  echo "GIT_DATE \"${GIT_DATE}\""
fi

OUTPUT_GIT_DESCRIBE="$(git describe 2>/dev/null)"
if [ -z "${OUTPUT_GIT_DESCRIBE}" ]; then
  OUTPUT_GIT_DESCRIBE="${GIT_VERSION}"
fi
if [ ! -z "${OUTPUT_GIT_DESCRIBE}" ]; then
  echo "GIT_DESCRIBE \"${OUTPUT_GIT_DESCRIBE}\""
fi
