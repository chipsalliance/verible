"""Tool for obfuscating verilog
"""

licenses(["notice"])

cc_binary(
    name = "verible-verilog-obfuscate",
    srcs = ["verilog_obfuscate.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/strings:obfuscator",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//verilog/analysis:extractors",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/transform:obfuscate",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)

sh_test(
    name = "obfuscate_test",
    size = "small",
    srcs = ["obfuscate_test.sh"],
    args = [
        "$(location :verible-verilog-obfuscate)",
        "$(location //verilog/tools/diff:verible-verilog-diff)",
    ],
    data = [
        ":verible-verilog-obfuscate",
        "//verilog/tools/diff:verible-verilog-diff",
    ],
)
