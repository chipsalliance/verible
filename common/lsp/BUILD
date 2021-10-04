# This package contains the basic language server protocol [1] implementation
# needed in language specific parts to provide LSP services.
# LSP is using JSON-PRC [2] for the RPC protocol.
# [1]: https://microsoft.github.io/language-server-protocol/specification
# [2]: https://www.jsonrpc.org/specification

licenses(["notice"])

package(
    default_visibility = [
        "//:__subpackages__",
    ],
)

cc_library(
    name = "message-stream-splitter",
    hdrs = ["message-stream-splitter.h"],
    srcs = ["message-stream-splitter.cc"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/status",
    ],
)


cc_test(
    name = "message-stream-splitter_test",
    srcs = ["message-stream-splitter_test.cc"],
    deps = [
        ":message-stream-splitter",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "json-rpc-dispatcher",
    hdrs = ["json-rpc-dispatcher.h"],
    srcs = ["json-rpc-dispatcher.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/status",
        "@jsonhpp//:jsonhpp",
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
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
