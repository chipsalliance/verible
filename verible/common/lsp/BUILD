# This package contains the basic language server protocol [1] implementation
# needed in language specific parts to provide LSP services.
# LSP is using JSON-PRC [2] for the RPC protocol, the transport layer chunks
# messages with header+body blocks, similar to HTTP.
#
# All libraries needed to build a language server have the public visibility
# as they are very useful in itself to build language servers in other projects.
#
# [1]: https://microsoft.github.io/language-server-protocol/specification
# [2]: https://www.jsonrpc.org/specification

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verible/verilog/tools/ls:__subpackages__",
    ],
    features = ["layering_check"],
)

cc_library(
    name = "message-stream-splitter",
    srcs = ["message-stream-splitter.cc"],
    hdrs = ["message-stream-splitter.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//verible/common/util:status-macros",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_test(
    name = "message-stream-splitter_test",
    srcs = ["message-stream-splitter_test.cc"],
    deps = [
        ":message-stream-splitter",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "json-rpc-dispatcher",
    srcs = ["json-rpc-dispatcher.cc"],
    hdrs = ["json-rpc-dispatcher.h"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    visibility = ["//visibility:public"],
    deps = [
        "//verible/common/util:logging",
        "@abseil-cpp//absl/strings:string_view",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_test(
    name = "json-rpc-dispatcher_test",
    srcs = ["json-rpc-dispatcher_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":json-rpc-dispatcher",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@nlohmann_json//:singleheader-json",
    ],
)

genrule(
    name = "lsp-protocol-gen",
    srcs = ["lsp-protocol.yaml"],
    outs = ["lsp-protocol.h"],
    cmd = "$(location :jcxxgen) --class_namespace verible::lsp --output $@ $<",
    tools = [":jcxxgen"],
)

cc_library(
    name = "lsp-protocol",
    hdrs = ["lsp-protocol.h"],
    visibility = ["//visibility:public"],
    deps = ["@nlohmann_json//:singleheader-json"],
)

cc_library(
    name = "lsp-protocol-enums",
    hdrs = ["lsp-protocol-enums.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "lsp-protocol-operators",
    hdrs = ["lsp-protocol-operators.h"],
    visibility = ["//visibility:public"],
    deps = [":lsp-protocol"],
)

cc_test(
    name = "lsp-protocol-operators_test",
    srcs = ["lsp-protocol-operators_test.cc"],
    deps = [
        ":lsp-protocol",
        ":lsp-protocol-operators",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "lsp-text-buffer",
    srcs = ["lsp-text-buffer.cc"],
    hdrs = ["lsp-text-buffer.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":json-rpc-dispatcher",
        ":lsp-protocol",
        "//verible/common/strings:utf8",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_test(
    name = "lsp-text-buffer_test",
    srcs = ["lsp-text-buffer_test.cc"],
    deps = [
        ":json-rpc-dispatcher",
        ":lsp-protocol",
        ":lsp-text-buffer",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "lsp-file-utils",
    srcs = ["lsp-file-utils.cc"],
    hdrs = ["lsp-file-utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_test(
    name = "lsp-file-utils_test",
    srcs = ["lsp-file-utils_test.cc"],
    deps = [
        ":lsp-file-utils",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_binary(
    name = "json-rpc-expect",
    srcs = ["json-rpc-expect.cc"],
    deps = [
        ":message-stream-splitter",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@nlohmann_json//:singleheader-json",
    ],
)

cc_binary(
    name = "dummy-ls",
    srcs = ["dummy-ls.cc"],
    deps = [
        ":json-rpc-dispatcher",
        ":lsp-protocol",
        ":lsp-text-buffer",
        ":message-stream-splitter",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:string_view",
        "@nlohmann_json//:singleheader-json",
    ],
)

sh_test_with_runfiles_lib(
    name = "dummy-ls_test",
    size = "small",
    srcs = ["dummy-ls_test.sh"],
    args = [
        "$(location :dummy-ls)",
        "$(location :json-rpc-expect)",
    ],
    data = [
        ":dummy-ls",
        ":json-rpc-expect",
    ],
    deps = [],
)

cc_binary(
    name = "jcxxgen",
    srcs = ["jcxxgen.cc"],
    visibility = ["//visibility:private"],
    deps = [
        "//verible/common/util:init-command-line",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/strings",
        "@re2",
    ],
)

genrule(
    name = "jcxxgen-testfile-gen",
    srcs = ["jcxxgen-testfile.yaml"],
    outs = ["jcxxgen-testfile.h"],
    cmd = "$(location :jcxxgen) --class_namespace verible::test --output $@ $<",
    tools = [":jcxxgen"],
)

cc_library(
    name = "jcxxgen-testfile",
    hdrs = ["jcxxgen-testfile.h"],
    deps = ["@nlohmann_json//:singleheader-json"],
)

cc_test(
    name = "jcxxgen_test",
    srcs = ["jcxxgen_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":jcxxgen-testfile",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@nlohmann_json//:singleheader-json",
    ],
)
