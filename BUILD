# Toplevel build-file for Verible libraries and tools.
# To compile the tools, invoke
#  bazel build ...
# Run tests with
#  bazel test ...

load("@com_github_google_rules_install//installer:def.bzl", "installer")
load("@com_grail_bazel_compdb//:aspects.bzl", "compilation_database")

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
        "//verilog/tools/lint:verible-verilog-lint",
        "//verilog/tools/obfuscator:verible-verilog-obfuscate",
        "//verilog/tools/preprocessor:verible-verilog-preprocessor",
        "//verilog/tools/project:verible-verilog-project",
        "//verilog/tools/syntax:verible-verilog-syntax",
        "//verilog/tools/ls:verible-verilog-ls",
    ],
)

filegroup(
   name = "install-scripts",
   srcs = [
        "//common/tools:verible-transform-interactive",
        "//verilog/tools/formatter:verible-verilog-format-changed-lines-interactive",
        "//verilog/tools/formatter:git-verilog-format",
   ]
)

installer(
    name = "install",
    data = [
        ":install-binaries",
        ":install-scripts",
   ]
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

compilation_database(
    name = "compdb",
    # targets = [ ":install-binaries" ],
    # Unfortunately, compilation_database does not support filesets yet,
    # so expand them here manually.
    # https://github.com/grailbio/bazel-compilation-database/issues/84
   targets = [
        "//common/tools:verible-patch-tool",
        "//verilog/tools/diff:verible-verilog-diff",
        "//verilog/tools/formatter:verible-verilog-format",
        "//verilog/tools/kythe:verible-verilog-kythe-extractor",
        "//verilog/tools/lint:verible-verilog-lint",
        "//verilog/tools/obfuscator:verible-verilog-obfuscate",
        "//verilog/tools/preprocessor:verible-verilog-preprocessor",
        "//verilog/tools/project:verible-verilog-project",
        "//verilog/tools/syntax:verible-verilog-syntax",
        "//verilog/tools/ls:verible-verilog-ls",
        "//common/lsp:dummy-ls",
    ],

    # TODO: is there a way to essentially specify //... so that all tests
    # are included as well ?
)
