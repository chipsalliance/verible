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

# Adapter rule around the @rules_flex toolchain.
def genlex(name, src, out):
    """Generate C/C++ language source from lex file using Flex
    """

    # Note: some attributes such as (toolchains and name) cannot use select. So, although verbose, we create a
    # genrule for each condition here and copy the resulting file to the expected location.

    # For rules_flex default case
    native.genrule(
        name = name + "_default",
        srcs = [src],
        outs = [out + ".default"],
        cmd = "M4=$(M4) $(FLEX) --outfile=$(location " + out + ".default) $<",
        toolchains = [
            "@rules_flex//flex:current_flex_toolchain",
            "@rules_m4//m4:current_m4_toolchain",
        ],
        tags = ["manual"],
    )

    # For local flex case
    native.genrule(
        name = name + "_local",
        srcs = [src],
        outs = [out + ".local"],
        cmd = "flex --outfile=$(location " + out + ".local) $<",
        tags = ["manual"],
    )

    # For Windows case
    native.genrule(
        name = name + "_windows",
        srcs = [src],
        outs = [out + ".windows"],
        cmd = "win_flex.exe --outfile=$(location " + out + ".windows) $<",
        tags = ["manual"],
    )

    # Create a copy rule to get the final output
    native.genrule(
        name = name,
        srcs = select({
            "//bazel:use_local_flex_bison_enabled": [out + ".local"],
            "@platforms//os:windows": [out + ".windows"],
            "//conditions:default": [out + ".default"],
        }),
        outs = [out],
        cmd = "cp $< $@",
    )
