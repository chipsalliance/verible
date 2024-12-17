"""Tool for obfuscating verilog
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_binary(
    name = "verible-verilog-obfuscate",
    srcs = ["verilog-obfuscate.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        "//verible/common/strings:obfuscator",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/verilog/analysis:extractors",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "//verible/verilog/transform:obfuscate",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

sh_test_with_runfiles_lib(
    name = "obfuscate_test",
    size = "small",
    srcs = ["obfuscate_test.sh"],
    args = [
        "$(location :verible-verilog-obfuscate)",
        "$(location //verible/verilog/tools/diff:verible-verilog-diff)",
    ],
    data = [
        ":verible-verilog-obfuscate",
        "//verible/verilog/tools/diff:verible-verilog-diff",
    ],
)
