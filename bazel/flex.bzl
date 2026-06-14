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
    native.genrule(
        name = name + "_toolchain",
        srcs = [src],
        outs = ["toolchain/" + out],
        cmd = select({
            "@platforms//os:windows": "mkdir -p $(@D)/toolchain && win_flex.exe --outfile=$@ $<",
            "//conditions:default": "mkdir -p $(@D)/toolchain && M4=$(M4) $(FLEX) --outfile=$@ $<",
        }),
        toolchains = [
            "@rules_flex//flex:current_flex_toolchain",
            "@rules_m4//m4:current_m4_toolchain",
        ],
        tags = ["manual"],
    )

    native.genrule(
        name = name + "_local",
        srcs = [src],
        outs = ["local/" + out],
        cmd = select({
            "@platforms//os:windows": "mkdir -p $(@D)/local && win_flex.exe --outfile=$@ $<",
            "//conditions:default": "mkdir -p $(@D)/local && flex --outfile=$@ $<",
        }),
        tags = ["manual"],
    )

    native.genrule(
        name = name,
        srcs = select({
            "//bazel:use_local_flex_bison_enabled": ["local/" + out],
            "@platforms//os:windows": ["local/" + out],
            "//conditions:default": ["toolchain/" + out],
        }),
        outs = [out],
        cmd = "cp $< $@",
    )
