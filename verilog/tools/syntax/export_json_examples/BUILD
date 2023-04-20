load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

py_library(
    name = "verible-verilog-syntax-py",
    srcs = ["verible_verilog_syntax.py"],
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    imports = ["."],
    srcs_version = "PY3",
    deps = [
        "//third_party/py/dataclasses",
        "@python_anytree//:anytree",
    ],
)

py_test(
    name = "verible-verilog-syntax-py_test",
    size = "small",
    srcs = ["verible_verilog_syntax_test.py"],
    args = ["$(location //verilog/tools/syntax:verible-verilog-syntax)"],
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    main = "verible_verilog_syntax_test.py",
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [":verible-verilog-syntax-py"],
)

py_binary(
    name = "print-modules",
    srcs = ["print_modules.py"],
    args = ["$(location //verilog/tools/syntax:verible-verilog-syntax)"],
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    main = "print_modules.py",
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":verible-verilog-syntax-py",
        "@python_anytree//:anytree",
    ],
)

py_binary(
    name = "print-tree",
    srcs = ["print_tree.py"],
    args = ["$(location //verilog/tools/syntax:verible-verilog-syntax)"],
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    main = "print_tree.py",
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":verible-verilog-syntax-py",
        "@python_anytree//:anytree",
    ],
)
