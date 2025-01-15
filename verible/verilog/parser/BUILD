# This package implements a SystemVerilog lexer and parser.

load("//bazel:bison.bzl", "genyacc")
load("//bazel:flex.bzl", "genlex")
load(
    "//verible/common/parser:yacc.bzl",
    "record_recovered_syntax_errors",
    "std_move_parser_symbols",
)

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verible/verilog:__subpackages__",
    ],
    # Not yet enabled, lexer does not find FlexLexer.h
    #features = ["layering_check"],
)

genlex(
    name = "verilog-lex",
    src = "verilog.lex",
    out = "verilog.yy.cc",
)

cc_library(
    name = "verilog-lexer",
    srcs = [
        "verilog.yy.cc",
        "verilog-lexer.cc",
    ],
    hdrs = ["verilog-lexer.h"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            "-Wno-implicit-fallthrough",
        ],
    }),
    deps = [
        ":verilog-token-enum",
        "//bazel:flex",
        "//verible/common/lexer:flex-lexer-adapter",
        "//verible/common/text:token-info",
    ],
)

cc_test(
    name = "verilog-lexer_test",
    size = "small",
    srcs = ["verilog-lexer_test.cc"],
    deps = [
        ":verilog-lexer",
        ":verilog-token-enum",
        "//verible/common/lexer:lexer-test-util",
        "//verible/common/text:token-info",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

# To reduce cyclic header dependencies, split out verilog.tab.hh into:
# 1) enumeration only header (depends on nothing else)
# 2) parser prototype header (depends on parser parameter type)
# Do not use verilog.tab.hh directly anywhere.
# Even the comments in bison-generated files suggest they may be inclined
# to separate these headers in the future.  This is a workaround until then.
genrule(
    name = "gen-verilog-token-enum",
    srcs = ["verilog.tab.hh"],
    outs = ["verilog-token-enum.h"],
    # Needs own include-guard.
    cmd = "{ echo '#ifndef VERIBLE_VERILOG_TOKEN_ENUM_H_' ; " +
          "echo '#define VERIBLE_VERILOG_TOKEN_ENUM_H_' ; " +
          "echo '// DO NOT EDIT -- generated from $<' ; " +
          # Grab the enumeration definition.
          "sed -n '/#ifndef VERILOG_TOKENTYPE/,/#endif/p' $< ; " +
          "echo '#endif  // VERIBLE_VERILOG_VERILOG_TOKEN_ENUM_H_' ;} > $@",
)

cc_library(
    name = "verilog-token-enum",
    hdrs = ["verilog-token-enum.h"],
)

genrule(
    name = "verilog-parse-interface",
    srcs = ["verilog.tab.hh"],
    outs = ["verilog-parse-interface.h"],
    # Already contains include-guard from verilog.tab.hh
    cmd = "{ echo '// DO NOT EDIT -- generated from $<' ; " +
          # Filter out the enumeration definition.
          "sed -e '/#ifndef YYTOKENTYPE/,/#endif/d' $< ;} > $@",
)

genyacc(
    name = "verilog-y",
    src = "verilog.y",
    extra_options = [
        "--report-file=$(location verilog.output)",
        "--graph=$(location verilog.dot)",
    ],
    extra_outs = [
        "verilog.output",
        "verilog.dot",
    ],
    header_out = "verilog.tab.hh",  # Do not use directly, see next comment.
    source_out = "verilog.tab.cc",
)

std_move_parser_symbols(
    name = "verilog-y-moved",
    src = "verilog.tab.cc",
    out = "verilog-moved.tab.cc",
)

record_recovered_syntax_errors(
    name = "verilog-y-final",
    src = "verilog-moved.tab.cc",
    out = "verilog-final.tab.cc",
)

cc_library(
    name = "verilog-y-cc",
    srcs = ["verilog-final.tab.cc"],
    hdrs = [
        "verilog.tab.hh",
        "verilog-parse-interface.h",
    ],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            "-Wno-implicit-fallthrough",
            "-Wno-type-limits",
            "-Wno-unreachable-code",
        ],
    }),
    deps = [
        "//verible/common/parser:bison-parser-common",
        "//verible/common/parser:parser-param",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/CST:DPI",
        "//verible/verilog/CST:declaration",
        "//verible/verilog/CST:expression",
        "//verible/verilog/CST:functions",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:parameters",
        "//verible/verilog/CST:port",
        "//verible/verilog/CST:type",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/CST:verilog-treebuilder-utils",
    ],
    alwayslink = 1,
)

cc_library(
    name = "verilog-parser",
    srcs = ["verilog-parser.cc"],
    hdrs = ["verilog-parser.h"],
    deps = [
        ":verilog-y-cc",
        "//verible/common/parser:bison-parser-adapter",
        "//verible/common/parser:parse",
        "//verible/common/parser:parser-param",
        "//verible/common/util:value-saver",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "verilog-lexical-context",
    srcs = ["verilog-lexical-context.cc"],
    hdrs = ["verilog-lexical-context.h"],
    deps = [
        ":verilog-token-enum",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:logging",
        "//verible/common/util:with-reason",
    ],
)

cc_test(
    name = "verilog-parser_test",
    size = "small",
    srcs = ["verilog-parser_test.cc"],
    deps = [
        ":verilog-token-enum",
        "//verible/common/parser:bison-parser-common",
        "//verible/common/parser:parser-test-util",
        "//verible/common/text:constants",
        "//verible/common/text:parser-verifier",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:token-info-test-util",
        "//verible/common/util:logging",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-lexical-context_test",
    srcs = ["verilog-lexical-context_test.cc"],
    deps = [
        ":verilog-lexical-context",
        ":verilog-parser",
        ":verilog-token-enum",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-token-classifications",
    srcs = ["verilog-token-classifications.cc"],
    hdrs = ["verilog-token-classifications.h"],
    deps = [
        ":verilog-token-enum",
    ],
)

cc_test(
    name = "verilog-token-classifications_test",
    srcs = ["verilog-token-classifications_test.cc"],
    deps = [
        ":verilog-token-classifications",
        ":verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-token",
    srcs = ["verilog-token.cc"],
    hdrs = ["verilog-token.h"],
    deps = [
        ":verilog-parser",
        ":verilog-token-enum",
    ],
)
