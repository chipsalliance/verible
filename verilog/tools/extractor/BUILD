# 'verilog_syntax' is a program for checking Verilog/SystemVerilog syntax.

licenses(["notice"])

genrule(
    name = "verilog_extractor_types_foreach_gen",
    srcs = ["verilog_extractor_types.h"],
    outs = ["verilog_extractor_types_foreach-gen.inc"],
    cmd = "sed -n -e '/BEGIN GENERATE/,/END GENERATE/p' $< | " +
          "grep -v GENERATE | " +
          "sed -e 's|^ *|CONSIDER(|' -e 's| =.*,|,|' -e 's|,|)|' > $@",
)

cc_library(
    name = "verilog_extractor_types",
    srcs = [
        "verilog_extractor_types.cc",
        "verilog_extractor_types_foreach.inc",
        "verilog_extractor_types_foreach-gen.inc",
    ],
    hdrs = [
        "verilog_extractor_types.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "verible-verilog-extractor",
    srcs = [
        "verilog_extractor.cc",
        "verilog_extractor_types_foreach.inc",
        "verilog_extractor_types_foreach-gen.inc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//common/text:concrete_syntax_tree",
        "//common/text:parser_verifier",
        "//common/text:symbol",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:logging",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_tree_print",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/analysis/checkers:verilog_lint_rules",
        "//verilog/parser:verilog_parser",
        "//verilog/tools/extractor:verilog_extractor_types",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)
