# JSON for Modern C++ https://github.com/nlohmann/json
#
# This file is not directly needed anymore, but left
# here for now for projects still using a WORKSPACE
# reference.
#
# Should probably be removed around mid 2025.

licenses(["unencumbered"])  # Public Domain or MIT

exports_files(["LICENSE"])

cc_library(
    name = "singleheader-json",
    hdrs = [
        "single_include/nlohmann/json.hpp",
    ],
    includes = ["single_include"],
    visibility = ["//visibility:public"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
)
