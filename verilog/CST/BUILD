# This package contains utility functions that extract or operate on
# the concrete syntax tree (cst) structure built up by verilog.y.
# Any code that relies on the internal tree structure should be isolated in
# here.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verilog/analysis:__subpackages__",
        "//verilog/formatting:__subpackages__",
        "//verilog/parser:__subpackages__",
        "//verilog/tools/kythe:__pkg__",
        "//verilog/tools/ls:__pkg__",  # DocumentSymbol
        "//verilog/tools/syntax:__pkg__",  # for printing
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
        "//common/text:constants",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "verilog-nonterminals_test",
    srcs = ["verilog-nonterminals_test.cc"],
    deps = [
        ":verilog-nonterminals",
        "//common/text:constants",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis/matcher:matcher-builders",
        "//common/text:symbol",
        "//verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "verilog-matchers_test",
    srcs = ["verilog-matchers_test.cc"],
    deps = [
        ":verilog-matchers",
        ":verilog-treebuilder-utils",
        "//common/analysis/matcher:core-matchers",
        "//common/analysis/matcher:matcher-builders",
        "//common/analysis/matcher:matcher-test-utils",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-treebuilder-utils",
    srcs = ["verilog-treebuilder-utils.cc"],
    hdrs = ["verilog-treebuilder-utils.h"],
    deps = [
        ":verilog-nonterminals",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/util:logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "verilog-treebuilder-utils_test",
    srcs = ["verilog-treebuilder-utils_test.cc"],
    deps = [
        ":verilog-treebuilder-utils",
        "//common/text:tree-builder-test-util",
        "//common/text:tree-utils",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "match-test-utils",
    testonly = 1,
    srcs = ["match-test-utils.cc"],
    hdrs = ["match-test-utils.h"],
    deps = [
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:text-structure",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",  # for library testonly
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
    ],
)

cc_test(
    name = "constraints_test",
    srcs = ["constraints_test.cc"],
    deps = [
        ":constraints",
        ":match-test-utils",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "DPI",
    srcs = ["DPI.cc"],
    hdrs = ["DPI.h"],
    deps = [
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:symbol-ptr",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "DPI_test",
    srcs = ["DPI_test.cc"],
    deps = [
        ":DPI",
        ":match-test-utils",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:text-structure",
        "//common/util:logging",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:constants",
        "//common/text:symbol",
        "//common/text:symbol-ptr",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:container-util",
        "//verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "declaration_test",
    srcs = ["declaration_test.cc"],
    deps = [
        ":declaration",
        ":match-test-utils",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:text-structure",
        "//common/text:tree-utils",
        "//common/util:logging",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "dimensions",
    srcs = ["dimensions.cc"],
    hdrs = ["dimensions.h"],
    deps = [
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-utils",
    ],
)

cc_test(
    name = "dimensions_test",
    srcs = ["dimensions_test.cc"],
    deps = [
        ":dimensions",
        ":verilog-matchers",
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/preprocessor:verilog-preprocess",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-utils",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:concrete-syntax-leaf",
        "//common/text:text-structure",
        "//common/text:token-info-test-util",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "identifier",
    srcs = ["identifier.cc"],
    hdrs = ["identifier.h"],
    deps = [
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "identifier_test",
    srcs = ["identifier_test.cc"],
    deps = [
        ":identifier",
        ":match-test-utils",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:concrete-syntax-leaf",
        "//common/text:text-structure",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "macro",
    srcs = ["macro.cc"],
    hdrs = ["macro.h"],
    deps = [
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/text:visitors",
        "//common/util:logging",
        "//verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "macro_test",
    srcs = ["macro_test.cc"],
    deps = [
        ":macro",
        ":match-test-utils",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:text-structure",
        "//common/text:token-info-test-util",
        "//common/util:logging",
        "//common/util:range",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-utils",
        "//common/text:visitors",
    ],
)

cc_test(
    name = "class_test",
    srcs = ["class_test.cc"],
    deps = [
        ":class",
        ":match-test-utils",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:text-structure",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "module",
    srcs = ["module.cc"],
    hdrs = ["module.h"],
    deps = [
        ":verilog-matchers",  # fixdeps: keep
        ":verilog-nonterminals",
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
    ],
)

cc_test(
    name = "module_test",
    srcs = ["module_test.cc"],
    deps = [
        ":match-test-utils",
        ":module",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:text-structure",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis/matcher",
        "//common/analysis/matcher:inner-match-handlers",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:syntax-tree-context",
        "//common/text:text-structure",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis/matcher",
        "//common/analysis/matcher:inner-match-handlers",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
    ],
)

cc_test(
    name = "data_test",
    srcs = ["data_test.cc"],
    deps = [
        ":data",
        "//common/text:text-structure",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "package",
    srcs = ["package.cc"],
    hdrs = ["package.h"],
    deps = [
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "package_test",
    srcs = ["package_test.cc"],
    deps = [
        ":match-test-utils",
        ":package",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:concrete-syntax-tree",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "parameters_test",
    srcs = ["parameters_test.cc"],
    deps = [
        ":match-test-utils",
        ":parameters",
        ":verilog-nonterminals",
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-utils",
        "//common/util:logging",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:concrete-syntax-leaf",
        "//common/text:syntax-tree-context",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "seq-block",
    srcs = ["seq-block.cc"],
    hdrs = ["seq-block.h"],
    deps = [
        ":verilog-nonterminals",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/parser:verilog-token-enum",
    ],
)

cc_test(
    name = "seq-block_test",
    srcs = ["seq-block_test.cc"],
    deps = [
        ":seq-block",
        ":verilog-matchers",
        ":verilog-nonterminals",
        "//common/analysis:syntax-tree-search",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-utils",
        "//common/text:visitors",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/analysis/matcher:matcher-builders",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:text-structure",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:tree-utils",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:casts",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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
        "//common/analysis:syntax-tree-search",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:symbol-ptr",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/parser:verilog-token-enum",
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
        "//common/analysis:syntax-tree-search",
        "//common/analysis:syntax-tree-search-test-utils",
        "//common/text:text-structure",
        "//common/text:tree-utils",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "context-functions",
    hdrs = ["context-functions.h"],
    deps = [
        ":verilog-nonterminals",
        "//common/text:syntax-tree-context",
    ],
)

cc_library(
    name = "numbers",
    srcs = ["numbers.cc"],
    hdrs = ["numbers.h"],
    deps = [
        "//common/util:logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "verilog-tree-print",
    srcs = ["verilog-tree-print.cc"],
    hdrs = ["verilog-tree-print.h"],
    deps = [
        ":verilog-nonterminals",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:tree-utils",
        "//common/util:value-saver",
        "//verilog/parser:verilog-parser",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "verilog-tree-json",
    srcs = ["verilog-tree-json.cc"],
    hdrs = ["verilog-tree-json.h"],
    deps = [
        ":verilog-nonterminals",
        "//common/text:concrete-syntax-leaf",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:token-info",
        "//common/text:token-info-json",
        "//common/text:visitors",
        "//common/util:value-saver",
        "//verilog/parser:verilog-token",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings:string_view",
        "@jsonhpp//:singleheader-json",
    ],
)

cc_test(
    name = "context-functions_test",
    srcs = ["context-functions_test.cc"],
    deps = [
        ":context-functions",
        ":verilog-nonterminals",
        "//common/text:concrete-syntax-tree",
        "//common/text:symbol",
        "//common/text:syntax-tree-context",
        "//common/text:tree-builder-test-util",
        "//common/util:casts",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "numbers_test",
    srcs = ["numbers_test.cc"],
    deps = [
        ":numbers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-tree-print_test",
    srcs = ["verilog-tree-print_test.cc"],
    deps = [
        ":verilog-tree-print",
        "//common/text:symbol",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "verilog-tree-json_test",
    srcs = ["verilog-tree-json_test.cc"],
    deps = [
        ":verilog-tree-json",
        "//common/text:symbol",
        "//common/util:logging",
        "//verilog/analysis:verilog-analyzer",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@jsonhpp//:singleheader-json",
    ],
)
