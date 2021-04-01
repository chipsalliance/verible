"""
Rules for adding './configure && make' style dependencies.
"""

load("@rules_foreign_cc//tools/build_defs:configure.bzl", "configure_make")

licenses(["notice"])  # Apache v2.0

package(
    default_visibility = [
        "//:__subpackages__",
    ],
)

exports_files([
    "bison.bzl",
    "flex.bzl",
])

configure_make(
    name = "m4",
    binaries = [
        "m4",
    ],
    configure_options = [
        "--disable-nls",
    ],
    lib_source = "@m4//:all",
)

filegroup(
    name = "m4_bin",
    srcs = [":m4"],
    output_group = "m4",
)

configure_make(
    name = "bison",
    binaries = [
        "bison",
        "yacc",
    ],
    configure_env_vars = {
        "M4": "$$EXT_BUILD_DEPS$$/m4/bin/m4",
        "CC_FOR_BUILD": "$$CC$$",
    },
    lib_source = "@bison//:all",
    static_libraries = ["liby.a"],
    deps = [":m4"],
)

filegroup(
    name = "bison_bin",
    srcs = [":bison"],
    output_group = "bison",
)
