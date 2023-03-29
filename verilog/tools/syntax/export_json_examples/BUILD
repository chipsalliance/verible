load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

py_library(
    name = "verible_verilog_syntax_py",
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
    name = "verible_verilog_syntax_py_test",
    size = "small",
    srcs = ["verible_verilog_syntax_test.py"],
    args = ["$(location //verilog/tools/syntax:verible-verilog-syntax)"],
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    main = "verible_verilog_syntax_test.py",
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [":verible_verilog_syntax_py"],
)

py_binary(
    name = "print_modules",
    srcs = ["print_modules.py"],
    args = ["$(location //verilog/tools/syntax:verible-verilog-syntax)"],
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    main = "print_modules.py",
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":verible_verilog_syntax_py",
        "@python_anytree//:anytree",
    ],
)

py_binary(
    name = "print_tree",
    srcs = ["print_tree.py"],
    args = ["$(location //verilog/tools/syntax:verible-verilog-syntax)"],
    data = ["//verilog/tools/syntax:verible-verilog-syntax"],
    main = "print_tree.py",
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":verible_verilog_syntax_py",
        "@python_anytree//:anytree",
    ],
)
