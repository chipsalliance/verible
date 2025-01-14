"""Tool for preprocessing Verilog
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_binary(
    name = "verible-verilog-preprocessor",
    srcs = ["verilog-preprocessor.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        "//verible/common/text:token-stream-view",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/common/util:status-macros",
        "//verible/common/util:subcommand",
        "//verible/verilog/analysis:flow-tree",
        "//verible/verilog/analysis:verilog-filelist",
        "//verible/verilog/analysis:verilog-project",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "//verible/verilog/transform:strip-comments",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

sh_test_with_runfiles_lib(
    name = "verilog-preprocessor_test",
    size = "small",
    srcs = ["verilog_preprocessor_test.sh"],
    args = ["$(location :verible-verilog-preprocessor)"],
    data = [":verible-verilog-preprocessor"],
    deps = [],
)
