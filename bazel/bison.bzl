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

"""Bazel rule to run bison
"""

load("@bison//:bison.bzl", "bison")

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

    bison(
        name = name,
        srcs = [src],
        outs = [header_out, source_out] + extra_outs,
        args = [
            "--output-file=$(location " + source_out + ")",
            "--defines=$(location " + header_out + ")",
        ] + extra_options + ["$(location " + src + ")"],
    )
