load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

package(
    default_visibility = ["//visibility:public"],
    features = ["layering_check"],
)

proto_library(
    name = "storage_proto",
    srcs = ["storage.proto"],
)

cc_proto_library(
    name = "storage_cc_proto",
    deps = [":storage_proto"],
)

proto_library(
    name = "analysis_proto",
    srcs = ["analysis.proto"],
    deps = [
        ":storage_proto",
        "@protobuf//:any_proto",
        "@protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "analysis_cc_proto",
    deps = [":analysis_proto"],
)
