#!/usr/bin/env bash
# Copyright 2021-2025 The Verible Authors.
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

# Which bazel and bant to use can be chosen by environment variables
BAZEL=${BAZEL:-bazel}
BANT=$($(dirname $0)/get-bant-path.sh)

BAZEL_OPTS="-c opt --noshow_progress --remote_download_outputs=all"

# Trigger necessary fetches from MODULE.bazel
for f in abseil-cpp nlohmann_json protobuf re2 rules_flex zlib googletest ; do
  "${BAZEL}" fetch --repo "@$f" > /dev/null 2>&1
done

# Bazel-build all targets that generate files, so that they can be
# seen in dependency analysis.
"${BAZEL}" build ${BAZEL_OPTS} \
           $(${BANT} list-targets -g "genrule|cc_proto_library" -m -c3 ...)

# bant does not distinguish the compile flags per file yet, so instead of
# a compile_commands.json, we can just as well create a simpler
# compile_flags.txt which is easier to digest for all kinds of tools anyway.
${BANT} compile-flags -o compile_flags.txt

# Bant does not see the flex dependency inside the toolchain yet.
for d in bazel-out/../../../external/*flex*/src/FlexLexer.h ; do
  echo "-I$(dirname $d)" >> compile_flags.txt
done
