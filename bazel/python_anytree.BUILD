load("@rules_python//python:defs.bzl", "py_library")

licenses(["notice"]) # Apache License 2.0

py_library(
    name = "anytree",
    srcs = [
        "anytree/__init__.py",
        "anytree/cachedsearch.py",
        "anytree/dotexport.py",
        "anytree/exporter/__init__.py",
        "anytree/exporter/dictexporter.py",
        "anytree/exporter/dotexporter.py",
        "anytree/exporter/jsonexporter.py",
        "anytree/importer/__init__.py",
        "anytree/importer/dictimporter.py",
        "anytree/importer/jsonimporter.py",
        "anytree/iterators/__init__.py",
        "anytree/iterators/abstractiter.py",
        "anytree/iterators/levelordergroupiter.py",
        "anytree/iterators/levelorderiter.py",
        "anytree/iterators/postorderiter.py",
        "anytree/iterators/preorderiter.py",
        "anytree/iterators/zigzaggroupiter.py",
        "anytree/node/__init__.py",
        "anytree/node/anynode.py",
        "anytree/node/exceptions.py",
        "anytree/node/node.py",
        "anytree/node/nodemixin.py",
        "anytree/node/symlinknode.py",
        "anytree/node/symlinknodemixin.py",
        "anytree/node/util.py",
        "anytree/render.py",
        "anytree/resolver.py",
        "anytree/search.py",
        "anytree/util/__init__.py",
        "anytree/walker.py",
    ],
    visibility = ["//visibility:public"],
    srcs_version = "PY3",
    imports = ["."],
    deps = [
        "@python_six//:six",
    ]
)
