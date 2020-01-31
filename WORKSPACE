workspace(name = "com_google_verible")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "com_google_absl",
    sha256 = "0b62fc2d00c2b2bc3761a892a17ac3b8af3578bd28535d90b4c914b0a7460d4e",
    strip_prefix = "abseil-cpp-20190808",
    urls = ["https://github.com/abseil/abseil-cpp/archive/20190808.zip"],
)

http_archive(
    name = "com_google_googletest",
    sha256 = "94c634d499558a76fa649edb13721dce6e98fb1e7018dfaeba3cd7a083945e91",
    strip_prefix = "googletest-release-1.10.0",
    urls = ["https://github.com/google/googletest/archive/release-1.10.0.zip"],
)

http_archive(
    name = "rules_cc",
    sha256 = "69fb4b965c538509324960817965791761d57010f42bf12ce9769c4259c7d018",
    strip_prefix = "rules_cc-e7c97c3af74e279a5db516a19f642e862ff58548",
    urls = ["https://github.com/bazelbuild/rules_cc/archive/e7c97c3af74e279a5db516a19f642e862ff58548.zip"],
)

#
# External tools needed
#

###
# Setup `rules_foreign_cc`
# This package allows us to take source-dependencies on non-bazelified packages.
# In particular, it supports `./configure && make` style packages.
###
http_archive(
    name = "rules_foreign_cc",
    sha256 = "ab266a13f5f695c898052271af860bf4928fb2ef6a333f7b63076b81271e4342",
    strip_prefix = "rules_foreign_cc-6bb0536452eaca3bad20c21ba6e7968d2eda004d",
    urls = ["https://github.com/bazelbuild/rules_foreign_cc/archive/6bb0536452eaca3bad20c21ba6e7968d2eda004d.zip"],
)

load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

all_content = """filegroup(name = "all", srcs = glob(["**"]), visibility = ["//visibility:public"])"""

http_archive(
    name = "bison",
    build_file_content = all_content,
    sha256 = "0b36200b9868ee289b78cefd1199496b02b76899bbb7e84ff1c0733a991313d1",
    strip_prefix = "bison-3.5",
    urls = ["https://ftp.gnu.org/gnu/bison/bison-3.5.tar.gz"],
)

http_archive(
    name = "flex",
    build_file_content = all_content,
    patch_args = ["-p1"],
    patches = ["@com_google_verible//bazel:flex.patch"],
    sha256 = "e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995",
    strip_prefix = "flex-2.6.4",
    urls = ["https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz"],
)

http_archive(
    name = "m4",
    build_file_content = all_content,
    patch_args = ["-p1"],
    patches = ["@com_google_verible//bazel:m4.patch"],
    sha256 = "ab2633921a5cd38e48797bf5521ad259bdc4b979078034a3b790d7fec5493fab",
    strip_prefix = "m4-1.4.18",
    urls = ["https://ftp.gnu.org/gnu/m4/m4-1.4.18.tar.gz"],
)
