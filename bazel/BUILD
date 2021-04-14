"""
Rules for adding './configure && make' style dependencies.
"""

licenses(["notice"])  # Apache v2.0

package(
    default_visibility = [
        "//:__subpackages__",
    ],
)

cc_library(
    name = "flex",
    deps = [
        "@rules_flex//flex:current_flex_toolchain",  # For FlexLexer.h
    ],
)

exports_files([
    "bison.bzl",
    "flex.bzl",
])
