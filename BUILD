# Toplevel build-file for Verible libraries and tools.
# To compile the tools, invoke
#  bazel build ...
# Run tests with
#  bazel test ...

load("@com_github_google_rules_install//installer:def.bzl", "installer")
#load("@com_grail_bazel_compdb//:defs.bzl", "compilation_database")
load("//bazel:all_compdb_rules.bzl", "ALL_COMPDB_RULES")

licenses(["notice"])  # Apache 2.0

exports_files([
    "LICENSE",
])

filegroup(
    name = "install-binaries",
    srcs = [
        "//common/tools:verible-patch-tool",
        "//verilog/tools/diff:verible-verilog-diff",
        "//verilog/tools/formatter:verible-verilog-format",
        "//verilog/tools/kythe:verible-verilog-kythe-extractor",
        "//verilog/tools/kythe:verible-verilog-kythe-kzip-writer",
        "//verilog/tools/lint:verible-verilog-lint",
        "//verilog/tools/ls:verible-verilog-ls",
        "//verilog/tools/obfuscator:verible-verilog-obfuscate",
        "//verilog/tools/preprocessor:verible-verilog-preprocessor",
        "//verilog/tools/project:verible-verilog-project",
        "//verilog/tools/syntax:verible-verilog-syntax",
    ],
)

filegroup(
    name = "install-scripts",
    srcs = [
        "//common/tools:verible-transform-interactive",
        "//verilog/tools/formatter:git-verilog-format",
        "//verilog/tools/formatter:verible-verilog-format-changed-lines-interactive",
    ],
)

installer(
    name = "install",
    data = [
        ":install-binaries",
        ":install-scripts",
    ],
)

genrule(
    name = "lint_doc",
    outs = ["documentation_verible_lint_rules.md"],
    cmd = "$(location //verilog/tools/lint:verible-verilog-lint) " +
          "--generate_markdown > $(OUTS)",
    tools = [
        "//verilog/tools/lint:verible-verilog-lint",
    ],
)

extra_action(
    name = "extractor",
    cmd = ("/opt/kythe/extractors/bazel_cxx_extractor " +
           "$(EXTRA_ACTION_FILE) $(output $(ACTION_ID).cxx.kzip) $(location :vnames.json)"),
    data = [":vnames.json"],
    out_templates = ["$(ACTION_ID).cxx.kzip"],
)

action_listener(
    name = "extract_cxx",
    extra_actions = [":extractor"],
    mnemonics = ["CppCompile"],
    visibility = ["//visibility:public"],
)

filegroup(
   name = "all",
   srcs = ALL_COMPDB_RULES,
   testonly = True,  # Allow to include testonly targets
)

# TODO:
# Testing currently without the compdb. Just the file-list above triggers
# issues on MacOs and Windows
#compilation_database(
#    name = "compdb",
#    targets = [ ":all" ],
#    testonly = True,    # We want targets that are dependent on testonly libs
#)
