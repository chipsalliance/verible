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

def _add_suffix_to_location_references(option, suffix):
    """Helper function to add a suffix to $(location ...) file names in options."""
    if "$(location " in option:
        # Extract the output file name
        start = option.find("$(location ") + len("$(location ")
        end = option.find(")", start)
        file_name = option[start:end]

        # Add suffix to the location reference
        return option.replace("$(location " + file_name + ")", "$(location " + file_name + suffix + ")")
    return option

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

    # Note: some attributes such as (toolchains and name) cannot use select. So, although verbose, we create a
    # genrule for each condition here and copy the resulting file to the expected location.

    # For rules_bison toolchain case
    native.genrule(
        name = name + "_default",
        srcs = [src],
        outs = [header_out + ".default", source_out + ".default"] + [out + ".default" for out in extra_outs],
        cmd = "M4=$(M4) $(BISON) --defines=$(location " + header_out + ".default) " +
              "--output-file=$(location " + source_out + ".default) " +
              " ".join([_add_suffix_to_location_references(opt, ".default") for opt in extra_options]) + " $<",
        toolchains = [
            "@rules_bison//bison:current_bison_toolchain",
            "@rules_m4//m4:current_m4_toolchain",
        ],
        tags = ["manual"],
    )

    # For local bison case
    native.genrule(
        name = name + "_local",
        srcs = [src],
        outs = [header_out + ".local", source_out + ".local"] + [out + ".local" for out in extra_outs],
        cmd = "bison --defines=$(location " + header_out + ".local) " +
              "--output-file=$(location " + source_out + ".local) " +
              " ".join([_add_suffix_to_location_references(opt, ".local") for opt in extra_options]) + " $<",
        tags = ["manual"],
    )

    # For Windows case
    native.genrule(
        name = name + "_windows",
        srcs = [src],
        outs = [header_out + ".windows", source_out + ".windows"] + [out + ".windows" for out in extra_outs],
        cmd = "win_bison.exe --defines=$(location " + header_out + ".windows) " +
              "--output-file=$(location " + source_out + ".windows) " +
              " ".join([_add_suffix_to_location_references(opt, ".windows") for opt in extra_options]) + " $<",
        tags = ["manual"],
    )

    # Create copy rules for each output
    native.genrule(
        name = name + "_copy_header",
        srcs = select({
            "//bazel:use_local_flex_bison_enabled": [header_out + ".local"],
            "@platforms//os:windows": [header_out + ".windows"],
            "//conditions:default": [header_out + ".default"],
        }),
        outs = [header_out],
        cmd = "cp $< $@",
    )

    native.genrule(
        name = name + "_copy_source",
        srcs = select({
            "//bazel:use_local_flex_bison_enabled": [source_out + ".local"],
            "@platforms//os:windows": [source_out + ".windows"],
            "//conditions:default": [source_out + ".default"],
        }),
        outs = [source_out],
        cmd = "cp $< $@",
    )

    # Create copy rules for each extra output
    for extra_out in extra_outs:
        sanitized_name = extra_out.replace(".", "_").replace("/", "_").replace("-", "_")
        native.genrule(
            name = name + "_copy_" + sanitized_name,
            srcs = select({
                "//bazel:use_local_flex_bison_enabled": [extra_out + ".local"],
                "@platforms//os:windows": [extra_out + ".windows"],
                "//conditions:default": [extra_out + ".default"],
            }),
            outs = [extra_out],
            cmd = "cp $< $@",
        )
