# Toplevel build-file for Verible libraries and tools.
# To compile the tools, invoke
#  bazel build ...
# Run tests with
#  bazel test ...

load("@com_github_google_rules_install//installer:def.bzl", "installer")
load("@rules_license//rules:license.bzl", "license")

package(
    default_applicable_licenses = [":license"],
    default_visibility = ["//visibility:public"],
)

# More machine-readable way to specify the license
license(
    name = "license",
    package_name = "verible",
    license_kind = "@rules_license//licenses/spdx:Apache-2.0",
    license_text = "LICENSE",
)

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
