# This package contains a SystemVerilog language server [1] implementation.
#
# [1]: https://microsoft.github.io/language-server-protocol/specification

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_library(
    name = "autoexpand",
    srcs = ["autoexpand.cc"],
    hdrs = ["autoexpand.h"],
    deps = [
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/strings:line-column-map",
        "//verible/common/strings:position",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/util:interval",
        "//verible/common/util:logging",
        "//verible/verilog/CST:declaration",
        "//verible/verilog/CST:dimensions",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:net",
        "//verible/verilog/CST:port",
        "//verible/verilog/CST:type",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/formatting:format-style",
        "//verible/verilog/formatting:format-style-init",
        "//verible/verilog/formatting:formatter",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/container:node_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
)

cc_library(
    name = "lsp-parse-buffer",
    srcs = ["lsp-parse-buffer.cc"],
    hdrs = ["lsp-parse-buffer.h"],
    deps = [
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/lsp:lsp-file-utils",
        "//verible/common/lsp:lsp-text-buffer",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/analysis:verilog-linter",
        "//verible/verilog/analysis:verilog-linter-configuration",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_test(
    name = "lsp-parse-buffer_test",
    srcs = ["lsp-parse-buffer_test.cc"],
    deps = [
        ":lsp-parse-buffer",
        "//verible/common/lsp:lsp-text-buffer",
        "//verible/common/text:text-structure",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verible-lsp-adapter",
    srcs = ["verible-lsp-adapter.cc"],
    hdrs = ["verible-lsp-adapter.h"],
    deps = [
        ":autoexpand",
        ":document-symbol-filler",
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//verible/common/analysis:file-analyzer",
        "//verible/common/analysis:lint-rule-status",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/lsp:lsp-protocol-enums",
        "//verible/common/lsp:lsp-protocol-operators",
        "//verible/common/strings:line-column-map",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:interval",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/analysis:verilog-linter",
        "//verible/verilog/formatting:format-style",
        "//verible/verilog/formatting:format-style-init",
        "//verible/verilog/formatting:formatter",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_library(
    name = "lsp-conversion",
    hdrs = ["lsp-conversion.h"],
    deps = [
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/strings:line-column-map",
    ],
)

cc_library(
    name = "document-symbol-filler",
    srcs = ["document-symbol-filler.cc"],
    hdrs = ["document-symbol-filler.h"],
    deps = [
        ":lsp-conversion",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/lsp:lsp-protocol-enums",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-utils",
        "//verible/common/text:visitors",
        "//verible/common/util:value-saver",
        "//verible/verilog/CST:class",
        "//verible/verilog/CST:functions",
        "//verible/verilog/CST:module",
        "//verible/verilog/CST:package",
        "//verible/verilog/CST:seq-block",
        "//verible/verilog/CST:verilog-nonterminals",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_library(
    name = "hover",
    srcs = ["hover.cc"],
    hdrs = ["hover.h"],
    deps = [
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "//verible/common/text:tree-context-visitor",
        "//verible/common/text:tree-utils",
        "//verible/common/util:casts",
        "//verible/common/util:range",
        "//verible/verilog/CST:seq-block",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:symbol-table",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "symbol-table-handler",
    srcs = ["symbol-table-handler.cc"],
    hdrs = ["symbol-table-handler.h"],
    # Can only add @abseil-cpp//absl/log:vlog_is_on after > absl 20240116
    # https://github.com/chipsalliance/verible/issues/2336
    features = ["-layering_check"],
    deps = [
        ":lsp-conversion",
        ":lsp-parse-buffer",
        "//verible/common/lsp:lsp-file-utils",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/strings:line-column-map",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/util:file-util",
        "//verible/common/util:iterator-adaptors",
        "//verible/common/util:logging",
        "//verible/common/util:range",
        "//verible/verilog/analysis:symbol-table",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/analysis:verilog-filelist",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/base:config",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/time",
        "@abseil-cpp//absl/types:optional",
    ],
)

cc_test(
    name = "symbol-table-handler_test",
    srcs = ["symbol-table-handler_test.cc"],
    deps = [
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//verible/common/lsp:lsp-file-utils",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/lsp:lsp-text-buffer",
        "//verible/common/util:file-util",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verilog-language-server",
    srcs = ["verilog-language-server.cc"],
    hdrs = ["verilog-language-server.h"],
    deps = [
        ":hover",
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        ":verible-lsp-adapter",
        "//verible/common/lsp:json-rpc-dispatcher",
        "//verible/common/lsp:lsp-file-utils",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/lsp:lsp-text-buffer",
        "//verible/common/lsp:message-stream-splitter",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_test(
    name = "verilog-language-server_test",
    srcs = ["verilog-language-server_test.cc"],
    deps = [
        ":verilog-language-server",
        "//verible/common/lsp:lsp-file-utils",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/lsp:lsp-protocol-enums",
        "//verible/common/strings:line-column-map",
        "//verible/common/util:file-util",
        "//verible/verilog/analysis:verilog-linter",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_binary(
    name = "verible-verilog-ls",
    srcs = ["verible-verilog-ls.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        ":verilog-language-server",
        "//verible/common/util:init-command-line",
        "@abseil-cpp//absl/status",
    ],
)

sh_test_with_runfiles_lib(
    name = "verible-verilog-ls_test",
    size = "small",
    srcs = ["verible-verilog-ls_test.sh"],
    args = [
        "$(location :verible-verilog-ls)",
        "$(location //verible/common/lsp:json-rpc-expect)",
    ],
    data = [
        ":verible-verilog-ls",
        "//verible/common/lsp:json-rpc-expect",
    ],
    deps = [],
)

cc_test(
    name = "autoexpand_test",
    srcs = ["autoexpand_test.cc"],
    deps = [
        ":autoexpand",
        ":lsp-parse-buffer",
        ":symbol-table-handler",
        "//verible/common/lsp:lsp-protocol",
        "//verible/common/lsp:lsp-text-buffer",
        "//verible/common/text:text-structure",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/analysis:verilog-project",
        "//verible/verilog/formatting:format-style",
        "//verible/verilog/formatting:format-style-init",
        "//verible/verilog/formatting:formatter",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@nlohmann_json//:singleheader-json",
    ],
)
