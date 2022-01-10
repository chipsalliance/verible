# Toplevel build-file for Verible libraries and tools.
# To compile the tools, invoke
#  bazel build ...
# Run tests with
#  bazel test ...

load("@com_github_google_rules_install//installer:def.bzl", "installer")
load("@com_grail_bazel_compdb//:aspects.bzl", "compilation_database")
load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@rules_pkg//:pkg.bzl", "pkg_deb")

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

# TODO(hzeller): would it be possible to easily bzl-script this put this
# together stripping all the individual prefices from the filegroup ?
pkg_tar(
    name = "install-patch-tool-bin",
    strip_prefix = "/common/tools",
    package_dir = "/usr/bin",
    srcs = ["//common/tools:verible-patch-tool"],
    mode = "0755",
)
pkg_tar(
    name = "install-diff-bin",
    strip_prefix = "/verilog/tools/diff",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/diff:verible-verilog-diff"],
    mode = "0755",
)
pkg_tar(
    name = "install-format-bin",
    strip_prefix = "/verilog/tools/formatter",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/formatter:verible-verilog-format"],
    mode = "0755",
)
pkg_tar(
    name = "install-kythe-bin",
    strip_prefix = "/verilog/tools/kythe",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/kythe:verible-verilog-kythe-extractor"],
    mode = "0755",
)
pkg_tar(
    name = "install-lint-bin",
    strip_prefix = "/verilog/tools/lint",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/lint:verible-verilog-lint"],
    mode = "0755",
)
pkg_tar(
    name = "install-ls-bin",
    strip_prefix = "/verilog/tools/ls",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/ls:verible-verilog-ls"],
    mode = "0755",
)
pkg_tar(
    name = "install-obfuscator-bin",
    strip_prefix = "/verilog/tools/obfuscator",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/obfuscator:verible-verilog-obfuscate"],
    mode = "0755",
)
pkg_tar(
    name = "install-preprocessor-bin",
    strip_prefix = "/verilog/tools/preprocessor",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/preprocessor:verible-verilog-preprocessor"],
    mode = "0755",
)
pkg_tar(
    name = "install-syntax-bin",
    strip_prefix = "/verilog/tools/syntax",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/syntax:verible-verilog-syntax"],
    mode = "0755",
)
pkg_tar(
    name = "install-transform-interactive-bin",
    strip_prefix = "/common/tools",
    package_dir = "/usr/bin",
    srcs = ["//common/tools:verible-transform-interactive"],
    mode = "0755",
)
pkg_tar(
    name = "install-format-changed-lines-bin",
    strip_prefix = "/verilog/tools/formatter",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/formatter:verible-verilog-format-changed-lines-interactive"],
    mode = "0755",
)
pkg_tar(
    name = "install-git-verilog-format-bin",
    strip_prefix = "/verilog/tools/formatter",
    package_dir = "/usr/bin",
    srcs = ["//verilog/tools/formatter:git-verilog-format"],
    mode = "0755",
)

pkg_tar(
    name = "install-data",
    extension = "tar.gz",
    deps = [
        ":install-patch-tool-bin",
        ":install-diff-bin",
        ":install-format-bin",
        ":install-kythe-bin",
        ":install-lint-bin",
        ":install-ls-bin",
        ":install-obfuscator-bin",
        ":install-preprocessor-bin",
        ":install-syntax-bin",
        ":install-transform-interactive-bin",
        ":install-format-changed-lines-bin",
    ],
)

pkg_deb(
    name = "verible-deb",
    architecture = "amd64",  # Can this come from build-env derived variable?
    built_using = "unzip (6.0.1)",
    data = ":install-data",
    depends = [],
    description_file = "README.md",
    homepage = "https://chipsalliance.github.io/verible/",
    maintainer = "The Verible Authors <verible-dev@googlegroups.com>",
    package = "verible",
    # TODO: how store bazel-out/volatile-status.txt into package variables ?
    version = "0.0.1000-abcdef",
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
