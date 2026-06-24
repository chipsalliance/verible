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

"""Bazel rule to run bison toolchain
"""

# Adapter rule around the @rules_bison toolchain.
def genyacc(
        name,
        src,
        header_out,
        source_out,
        extra_options = [],
        extra_outs = []):
    """Build rule for generating C or C++ sources with Bison.
    """
    all_outs = [header_out, source_out] + extra_outs

    def replace_outs(options, prefix):
        res = []
        for opt in options:
            for out in all_outs:
                opt = opt.replace("$(location %s)" % out, "$(location %s%s)" % (prefix, out))
            res.append(opt)
        return res

    native.genrule(
        name = name + "_toolchain",
        srcs = [src],
        outs = ["toolchain/" + out for out in all_outs],
        cmd = select({
            "@platforms//os:windows": "mkdir -p $(@D)/toolchain && win_bison.exe --defines=$(location toolchain/" + header_out + ") --output-file=$(location toolchain/" + source_out + ") " + " ".join(replace_outs(extra_options, "toolchain/")) + " $<",
            "//conditions:default": "mkdir -p $(@D)/toolchain && M4=$(M4) $(BISON) --defines=$(location toolchain/" + header_out + ") --output-file=$(location toolchain/" + source_out + ") " + " ".join(replace_outs(extra_options, "toolchain/")) + " $<",
        }),
        toolchains = [
            "@rules_bison//bison:current_bison_toolchain",
            "@rules_m4//m4:current_m4_toolchain",
        ],
        tags = ["manual"],
    )

    native.genrule(
        name = name + "_local",
        srcs = [src],
        outs = ["local/" + out for out in all_outs],
        cmd = select({
            "@platforms//os:windows": "mkdir -p $(@D)/local && win_bison.exe --defines=$(location local/" + header_out + ") --output-file=$(location local/" + source_out + ") " + " ".join(replace_outs(extra_options, "local/")) + " $<",
            "//conditions:default": "mkdir -p $(@D)/local && bison --defines=$(location local/" + header_out + ") --output-file=$(location local/" + source_out + ") " + " ".join(replace_outs(extra_options, "local/")) + " $<",
        }),
        tags = ["manual"],
    )

    def generate_copy_cmds(prefix):
        return " && ".join(["cp $(location %s%s) $(location %s)" % (prefix, out, out) for out in all_outs])

    native.genrule(
        name = name,
        srcs = select({
            "//bazel:use_local_flex_bison_enabled": ["local/" + out for out in all_outs],
            "@platforms//os:windows": ["local/" + out for out in all_outs],
            "//conditions:default": ["toolchain/" + out for out in all_outs],
        }),
        outs = all_outs,
        cmd = select({
            "//bazel:use_local_flex_bison_enabled": generate_copy_cmds("local/"),
            "@platforms//os:windows": generate_copy_cmds("local/"),
            "//conditions:default": generate_copy_cmds("toolchain/"),
        }),
    )
