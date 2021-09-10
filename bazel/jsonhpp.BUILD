# JSON for Modern C++ https://github.com/nlohmann/json

licenses(["unencumbered"])  # Public Domain or MIT

exports_files(["LICENSE"])

cc_library(
    name = "jsonhpp",
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
