# This package implements a SystemVerilog lexer and parser.

load("@rules_flex//flex:flex.bzl", "flex")
load("//bazel:bison.bzl", "genyacc")
load(
    "//common/parser:yacc.bzl",
    "record_recovered_syntax_errors",
    "std_move_parser_symbols",
)

licenses(["notice"])

package(
    default_visibility = [
        "//verilog:__subpackages__",
    ],
)

genrule(
    name = "verilog_lex",
    srcs  = ["verilog.lex"],
    outs = ["verilog.yy.cc"],
    cmd = "M4=$(M4) $(FLEX) --outfile=$@ $<",
    toolchains = [
        "@rules_flex//flex:current_flex_toolchain",
        "@rules_m4//m4:current_m4_toolchain",
    ],
)

cc_library(
    name = "verilog_lexer",
    srcs = [
        "verilog.yy.cc",
        "verilog_lexer.cc",
    ],
    hdrs = ["verilog_lexer.h"],
    copts = ["-Wno-implicit-fallthrough"],
    deps = [
        ":verilog_token_enum",
        "//common/lexer:flex_lexer_adapter",
        "//common/text:token_info",
        "@com_google_absl//absl/strings",
        "@rules_flex//flex:current_flex_toolchain",
    ],
)

cc_test(
    name = "verilog_lexer_unittest",
    size = "small",
    srcs = ["verilog_lexer_unittest.cc"],
    deps = [
        ":verilog_lexer",
        ":verilog_token_enum",
        "//common/lexer:lexer_test_util",
        "//common/text:token_info",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

# To reduce cyclic header dependencies, split out verilog.tab.hh into:
# 1) enumeration only header (depends on nothing else)
# 2) parser prototype header (depends on parser parameter type)
# Do not use verilog.tab.hh directly anywhere.
# Even the comments in bison-generated files suggest they may be inclined
# to separate these headers in the future.  This is a workaround until then.
genrule(
    name = "gen_verilog_token_enum",
    srcs = ["verilog.tab.hh"],
    outs = ["verilog_token_enum.h"],
    # Needs own include-guard.
    cmd = "{ echo '#ifndef VERIBLE_VERILOG_TOKEN_ENUM_H_' ; " +
          "echo '#define VERIBLE_VERILOG_TOKEN_ENUM_H_' ; " +
          "echo '// DO NOT EDIT -- generated from $<' ; " +
          # Grab the enumeration definition.
          "sed -n '/#ifndef VERILOG_TOKENTYPE/,/#endif/p' $< ; " +
          "echo '#endif  // VERIBLE_VERILOG_VERILOG_TOKEN_ENUM_H_' ;} > $@",
)

cc_library(
    name = "verilog_token_enum",
    hdrs = ["verilog_token_enum.h"],
)

genrule(
    name = "verilog_parse_interface",
    srcs = ["verilog.tab.hh"],
    outs = ["verilog_parse_interface.h"],
    # Already contains include-guard from verilog.tab.hh
    cmd = "{ echo '// DO NOT EDIT -- generated from $<' ; " +
          # Filter out the enumeration definition.
          "sed -e '/#ifndef YYTOKENTYPE/,/#endif/d' $< ;} > $@",
)

genrule(
    name = "verilog_y",
    srcs = ["verilog.y"],
    outs = ["verilog.tab.cc",
            "verilog.tab.hh",  # Do not use directly, see next comment.
           ],
    cmd = "M4=$(M4) $(BISON) --defines=$(location verilog.tab.hh) --output-file=$(location verilog.tab.cc) $<",
    toolchains = [
        "@rules_bison//bison:current_bison_toolchain",
        "@rules_m4//m4:current_m4_toolchain",
    ],
)

std_move_parser_symbols(
    name = "verilog_y_moved",
    src = "verilog.tab.cc",
    out = "verilog-moved.tab.cc",
)

record_recovered_syntax_errors(
    name = "verilog_y_final",
    src = "verilog-moved.tab.cc",
    out = "verilog-final.tab.cc",
)

cc_library(
    name = "verilog_y_cc",
    srcs = ["verilog-final.tab.cc"],
    hdrs = ["verilog_parse_interface.h"],
    copts = [
        "-Wno-implicit-fallthrough",
        "-Wno-type-limits",
    ],
    deps = [
        ":verilog_token_enum",
        "//common/parser:bison_parser_common",
        "//common/parser:parser_param",
        "//common/text:tree_utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/CST:DPI",
        "//verilog/CST:declaration",
        "//verilog/CST:expression",
        "//verilog/CST:functions",
        "//verilog/CST:module",
        "//verilog/CST:parameters",
        "//verilog/CST:port",
        "//verilog/CST:type",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/CST:verilog_treebuilder_utils",
    ],
)

cc_library(
    name = "verilog_parser",
    srcs = ["verilog_parser.cc"],
    hdrs = ["verilog_parser.h"],
    deps = [
        ":verilog_y_cc",  # fixdeps: keep
        "//common/parser:bison_parser_adapter",
        "//common/parser:parse",
        "//common/parser:parser_param",
        "//common/util:value_saver",
        "@com_google_absl//absl/flags:flag",
    ],
    alwayslink = 1,
)

cc_library(
    name = "verilog_lexical_context",
    srcs = ["verilog_lexical_context.cc"],
    hdrs = ["verilog_lexical_context.h"],
    deps = [
        ":verilog_token_enum",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:logging",
        "//common/util:with_reason",
    ],
)

cc_test(
    name = "verilog_parser_unittest",
    size = "small",
    srcs = ["verilog_parser_unittest.cc"],
    deps = [
        ":verilog_parser",
        ":verilog_token_enum",
        "//common/parser:bison_parser_common",
        "//common/parser:parser_test_util",
        "//common/text:constants",
        "//common/text:symbol",
        "//common/text:token_info",
        "//common/text:token_info_test_util",
        "//verilog/CST:verilog_nonterminals",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog_lexical_context_test",
    srcs = ["verilog_lexical_context_test.cc"],
    deps = [
        ":verilog_lexical_context",
        ":verilog_parser",
        ":verilog_token_enum",
        "//common/text:text_structure",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:logging",
        "//verilog/analysis:verilog_analyzer",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog_token_classifications",
    srcs = ["verilog_token_classifications.cc"],
    hdrs = ["verilog_token_classifications.h"],
    deps = [
        ":verilog_token_enum",
    ],
)

cc_test(
    name = "verilog_token_classifications_test",
    srcs = ["verilog_token_classifications_test.cc"],
    deps = [
        ":verilog_token_classifications",
        ":verilog_token_enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog_token",
    srcs = ["verilog_token.cc"],
    hdrs = ["verilog_token.h"],
    deps = [
        ":verilog_parser",
        ":verilog_token_enum",
        "@com_google_absl//absl/strings",
    ],
)
