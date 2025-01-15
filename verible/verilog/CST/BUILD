# This package contains utility functions that extract or operate on
# the concrete syntax tree (cst) structure built up by verilog.y.
# Any code that relies on the internal tree structure should be isolated in
# here.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verible/verilog/analysis:__subpackages__",
        "//verible/verilog/formatting:__subpackages__",
        "//verible/verilog/parser:__subpackages__",
        "//verible/verilog/tools/kythe:__pkg__",
        "//verible/verilog/tools/ls:__pkg__",  # DocumentSymbol
        "//verible/verilog/tools/syntax:__pkg__",  # for printing
    ],
    features = ["layering_check"],
)

# Generate foreach list for nonterminal enums.
# Libraries that need these generated include files should list them
# under srcs, even if they are included by headers.
genrule(
    name = "verilog-nonterminals-foreach-gen",
    srcs = ["verilog-nonterminals.h"],
    outs = ["verilog_nonterminals_foreach-gen.inc"],
    cmd = "sed -n -e '/BEGIN GENERATE/,/END GENERATE/p' $< | " +
          "grep -v GENERATE | " +
          "sed -e 's|^ *|CONSIDER(|' -e 's| =.*,|,|' -e 's|,|)|' > $@",
)

cc_library(
    name = "verilog-nonterminals",
    srcs = [
        "verilog-nonterminals.cc",
        "verilog_nonterminals_foreach-gen.inc",
    ],
    hdrs = [
        "verilog-nonterminals.h",
        "verilog_nonterminals_foreach.inc",
    ],
    deps = [
        "//verible/common/text:constants",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "verilog-nonterminals_test",
    srcs = ["verilog-nonterminals_test.cc"],
    deps = [
        ":verilog-nonterminals",
        "//verible/common/text:constants",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-matchers",
    srcs = [
        "verilog-matchers.cc",
        "verilog_nonterminals_foreach.inc",
        "verilog_nonterminals_foreach-gen.inc",
    ],
    hdrs = [
        "verilog-matchers.h",
    ],
    deps = [
        ":verilog-nonterminals",
        "//verible/common/analysis/matcher:matcher-builders",
        "//verible/common/text:symbol",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "verilog-matchers_test",
    srcs = ["verilog-matchers_test.cc"],
    deps = [
        ":verilog-matchers",
        ":verilog-treebuilder-utils",
        "//verible/common/analysis/matcher:core-matchers",
        "//verible/common/analysis/matcher:matcher-builders",
        "//verible/common/analysis/matcher:matcher-test-utils",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-treebuilder-utils",
    srcs = ["verilog-treebuilder-utils.cc"],
    hdrs = ["verilog-treebuilder-utils.h"],
    deps = [
        ":verilog-nonterminals",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "verilog-treebuilder-utils_test",
    srcs = ["verilog-treebuilder-utils_test.cc"],
    deps = [
        ":verilog-treebuilder-utils",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/text:tree-utils",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "match-test-utils",
    testonly = 1,
    srcs = ["match-test-utils.cc"],
    hdrs = ["match-test-utils.h"],
    deps = [
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:text-structure",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",  # for library testonly
    ],
)

cc_library(
    name = "constraints",
    srcs = ["constraints.cc"],
    hdrs = ["constraints.h"],
    deps = [
        ":identifier",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
    ],
)

cc_test(
    name = "constraints_test",
    srcs = ["constraints_test.cc"],
    deps = [
        ":constraints",
        ":match-test-utils",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "DPI",
    srcs = ["DPI.cc"],
    hdrs = ["DPI.h"],
    deps = [
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:symbol-ptr",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "DPI_test",
    srcs = ["DPI_test.cc"],
    deps = [
        ":DPI",
        ":match-test-utils",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "declaration",
    srcs = ["declaration.cc"],
    hdrs = ["declaration.h"],
    deps = [
        ":identifier",
        ":type",
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:constants",
        "//verible/common/text:symbol",
        "//verible/common/text:symbol-ptr",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:container-util",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "declaration_test",
    srcs = ["declaration_test.cc"],
    deps = [
        ":declaration",
        ":match-test-utils",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:text-structure",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "dimensions",
    srcs = ["dimensions.cc"],
    hdrs = ["dimensions.h"],
    deps = [
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-utils",
    ],
)

cc_test(
    name = "dimensions_test",
    srcs = ["dimensions_test.cc"],
    deps = [
        ":dimensions",
        ":verilog-matchers",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "expression",
    srcs = ["expression.cc"],
    hdrs = ["expression.h"],
    deps = [
        ":type",
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "expression_test",
    srcs = ["expression_test.cc"],
    deps = [
        ":expression",
        ":match-test-utils",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/preprocessor:verilog-preprocess",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "functions",
    srcs = ["functions.cc"],
    hdrs = ["functions.h"],
    deps = [
        ":identifier",
        ":type",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        ":verilog-treebuilder-utils",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-utils",
    ],
)

cc_test(
    name = "functions_test",
    srcs = ["functions_test.cc"],
    deps = [
        ":functions",
        ":identifier",
        ":match-test-utils",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info-test-util",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "identifier",
    srcs = ["identifier.cc"],
    hdrs = ["identifier.h"],
    deps = [
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "identifier_test",
    srcs = ["identifier_test.cc"],
    deps = [
        ":identifier",
        ":match-test-utils",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "macro",
    srcs = ["macro.cc"],
    hdrs = ["macro.h"],
    deps = [
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/text:visitors",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "macro_test",
    srcs = ["macro_test.cc"],
    deps = [
        ":macro",
        ":match-test-utils",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info-test-util",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "class",
    srcs = ["class.cc"],
    hdrs = ["class.h"],
    deps = [
        ":identifier",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-utils",
        "//verible/common/text:visitors",
    ],
)

cc_test(
    name = "class_test",
    srcs = ["class_test.cc"],
    deps = [
        ":class",
        ":match-test-utils",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "module",
    srcs = ["module.cc"],
    hdrs = ["module.h"],
    deps = [
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
    ],
)

cc_test(
    name = "module_test",
    srcs = ["module_test.cc"],
    deps = [
        ":match-test-utils",
        ":module",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "net",
    srcs = ["net.cc"],
    hdrs = ["net.h"],
    deps = [
        ":identifier",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:inner-match-handlers",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
    ],
)

cc_test(
    name = "net_test",
    srcs = ["net_test.cc"],
    deps = [
        ":declaration",
        ":match-test-utils",
        ":net",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "data",
    srcs = ["data.cc"],
    hdrs = ["data.h"],
    deps = [
        ":identifier",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:inner-match-handlers",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
    ],
)

cc_test(
    name = "data_test",
    srcs = ["data_test.cc"],
    deps = [
        ":data",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "package",
    srcs = ["package.cc"],
    hdrs = ["package.h"],
    deps = [
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "package_test",
    srcs = ["package_test.cc"],
    deps = [
        ":match-test-utils",
        ":package",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "parameters",
    srcs = ["parameters.cc"],
    hdrs = ["parameters.h"],
    deps = [
        ":identifier",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "parameters_test",
    srcs = ["parameters_test.cc"],
    deps = [
        ":match-test-utils",
        ":parameters",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "port",
    srcs = ["port.cc"],
    hdrs = ["port.h"],
    deps = [
        ":identifier",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
    ],
)

cc_test(
    name = "port_test",
    srcs = ["port_test.cc"],
    deps = [
        ":match-test-utils",
        ":port",
        ":type",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "seq-block",
    srcs = ["seq-block.cc"],
    hdrs = ["seq-block.h"],
    deps = [
        ":verilog-nonterminals",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "seq-block_test",
    srcs = ["seq-block_test.cc"],
    deps = [
        ":seq-block",
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "statement",
    srcs = ["statement.cc"],
    hdrs = ["statement.h"],
    deps = [
        ":declaration",
        ":identifier",
        ":type",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-utils",
        "//verible/common/text:visitors",
    ],
)

cc_test(
    name = "statement_test",
    srcs = ["statement_test.cc"],
    deps = [
        ":match-test-utils",
        ":statement",
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/analysis/matcher:matcher-builders",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "tasks",
    srcs = ["tasks.cc"],
    hdrs = ["tasks.h"],
    deps = [
        ":identifier",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:tree-utils",
    ],
)

cc_test(
    name = "tasks_test",
    srcs = ["tasks_test.cc"],
    deps = [
        ":identifier",
        ":match-test-utils",
        ":tasks",
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/parser:verilog-token-enum",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "type",
    srcs = ["type.cc"],
    hdrs = ["type.h"],
    deps = [
        ":identifier",
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:symbol-ptr",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "type_test",
    srcs = ["type_test.cc"],
    deps = [
        ":context-functions",
        ":declaration",
        ":expression",
        ":match-test-utils",
        ":type",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis:syntax-tree-search-test-utils",
        "//verible/common/text:text-structure",
        "//verible/common/text:tree-utils",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "context-functions",
    hdrs = ["context-functions.h"],
    deps = [
        ":verilog-nonterminals",
        "//verible/common/text:syntax-tree-context",
    ],
)

cc_library(
    name = "numbers",
    srcs = ["numbers.cc"],
    hdrs = ["numbers.h"],
    deps = [
        "//verible/common/util:logging",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "verilog-tree-print",
    srcs = ["verilog-tree-print.cc"],
    hdrs = ["verilog-tree-print.h"],
    deps = [
        ":verilog-nonterminals",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:value-saver",
        "//verible/verilog/parser:verilog-parser",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "verilog-tree-json",
    srcs = ["verilog-tree-json.cc"],
    hdrs = ["verilog-tree-json.h"],
    deps = [
        ":verilog-nonterminals",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:token-info-json",
        "//verible/common/text:visitors",
        "//verible/common/util:value-saver",
        "//verible/verilog/parser:verilog-token",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_test(
    name = "context-functions_test",
    srcs = ["context-functions_test.cc"],
    deps = [
        ":context-functions",
        ":verilog-nonterminals",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:syntax-tree-context",
        "//verible/common/text:tree-builder-test-util",
        "//verible/common/util:casts",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "numbers_test",
    srcs = ["numbers_test.cc"],
    deps = [
        ":numbers",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-tree-print_test",
    srcs = ["verilog-tree-print_test.cc"],
    deps = [
        ":verilog-tree-print",
        "//verible/common/text:symbol",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-tree-json_test",
    srcs = ["verilog-tree-json_test.cc"],
    deps = [
        ":verilog-tree-json",
        "//verible/common/text:symbol",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@nlohmann_json//:singleheader-json",
    ],
)
