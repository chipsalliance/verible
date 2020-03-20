"""Tool for obfuscating verilog
"""

licenses(["notice"])

cc_binary(
    name = "verilog_obfuscate",
    srcs = ["verilog_obfuscate.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/strings:obfuscator",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis:verilog_equivalence",
        "//verilog/transform:obfuscate",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)
