# -*- Python -*-
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

"""Bazel rule to run flex/bison on Windows
"""

# buildifier: disable=provider-params
FlexToolchainInfo = provider(
    fields = [
        "all_files",
        "flex_tool",
        "flex_lexer_h",
    ],
)

def _win_flex_toolchain_impl(ctx):
    flex_runfiles = ctx.attr.flex_tool[DefaultInfo].default_runfiles.files

    return platform_common.ToolchainInfo(
        flex_toolchain = FlexToolchainInfo(
            all_files = depset(
                direct = [ctx.executable.flex_tool],
                transitive = [flex_runfiles],
            ),
            flex_tool = ctx.attr.flex_tool.files_to_run,
            flex_lexer_h = ctx.file.flex_lexer_h,
        ),
    )

win_flex_toolchain = rule(
    _win_flex_toolchain_impl,
    attrs = {
        "flex_tool": attr.label(
            mandatory = True,
            executable = True,
            cfg = "host",
        ),
        "flex_lexer_h": attr.label(
            mandatory = True,
            allow_single_file = [".h"],
        ),
    },
)

# buildifier: disable=provider-params
BisonToolchainInfo = provider(
    fields = [
        "all_files",
        "bison_tool",
    ],
)

def _win_bison_toolchain_impl(ctx):
    bison_runfiles = ctx.attr.bison_tool[DefaultInfo].default_runfiles.files

    return platform_common.ToolchainInfo(
        bison_toolchain = BisonToolchainInfo(
            all_files = depset(
                direct = [ctx.executable.bison_tool],
                transitive = [bison_runfiles],
            ),
            bison_tool = ctx.attr.bison_tool.files_to_run,
        ),
    )

win_bison_toolchain = rule(
    _win_bison_toolchain_impl,
    attrs = {
        "bison_tool": attr.label(
            mandatory = True,
            executable = True,
            cfg = "host",
        ),
    },
)

def _remote_win_flex_bison(repository_ctx):
    repository_ctx.download_and_extract(
        url = repository_ctx.attr.url,
        sha256 = repository_ctx.attr.sha256,
    )

    repository_ctx.symlink(
        Label("@com_google_verible//bazel:win_flex_bison.BUILD"),
        "BUILD.bazel",
    )

remote_win_flex_bison = repository_rule(
    implementation = _remote_win_flex_bison,
    local = True,
    attrs = {
        "url": attr.string(mandatory = True),
        "sha256": attr.string(default = ""),
    },
)

def win_flex_configure(name, url, sha256 = ""):
    remote_win_flex_bison(
        name = name,
        url = url,
        sha256 = sha256,
    )

    native.register_toolchains(
        "@" + name + "//:flex_toolchain",
        "@" + name + "//:bison_toolchain",
    )
