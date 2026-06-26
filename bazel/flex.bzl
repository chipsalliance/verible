# -*- Python -*-
# Copyright 2017-2021 The Verible Authors.
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

"""Bazel rule to run flex toolchain
"""

def genlex(name, src, out):
    """Generate C/C++ language source from lex file using Flex
    """

    # Bazel 8 no longer allows select() in genrule.toolchains. Keep the
    # toolchain-provided executables on the configurable tools attribute, and
    # use PATH-provided flex/win_flex for the local Windows paths.
    default_cmd = "M4=$(execpath @rules_m4//m4:current_m4_toolchain) " + \
                  "$(execpath @rules_flex//flex:current_flex_toolchain) " + \
                  "--outfile=$@ $<"

    native.genrule(
        name = name,
        srcs = [src],
        outs = [out],
        cmd = select({
            "//bazel:use_local_flex_bison_enabled": "flex --outfile=$@ $<",
            "@platforms//os:windows": "win_flex.exe --outfile=$@ $<",
            "//conditions:default": default_cmd,
        }),
        tools = select({
            "//bazel:use_local_flex_bison_enabled": [],
            "@platforms//os:windows": [],
            "//conditions:default": [
                "@rules_flex//flex:current_flex_toolchain",
                "@rules_m4//m4:current_m4_toolchain",
            ],
        }),
    )
