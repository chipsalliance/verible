workspace(name = "com_google_verible")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# Bazel platform rules, needed as dependency to absl.
http_archive(
    name = "platforms",
    sha256 = "a879ea428c6d56ab0ec18224f976515948822451473a80d06c2e50af0bbe5121",
    strip_prefix = "platforms-da5541f26b7de1dc8e04c075c99df5351742a4a2",
    urls = ["https://github.com/bazelbuild/platforms/archive/da5541f26b7de1dc8e04c075c99df5351742a4a2.zip"],  # 2022-05-27
)

http_archive(
    name = "com_google_absl",
    # On MSVC's STL implementation, string_view cannot be constructed from
    # a string_view::iterator. This patch forces the use of absl's string_view
    # implementation to solve the issue
    patch_args = ["-p1"],
    patches = ["//bazel:absl.patch"],
    sha256 = "5b7640be0e119de1a9d941cb6b2607d76978eba5720196f1d4fc6de0421d2241",
    strip_prefix = "abseil-cpp-20220623.0",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.0.zip"],
)

# Googletest
# This requires RE2.
# Note this must use a commit from the `abseil` branch of the RE2 project.
# https://github.com/google/re2/tree/abseil
http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "2adb78cf4fafccaf3b2accef15389a279b1451a7fdf65529c866166b6d6ed9f1",
    strip_prefix = "re2-215bf4aa0bdc081862590463bc98a00bb2be48f2",
    urls = ["https://github.com/google/re2/archive/215bf4aa0bdc081862590463bc98a00bb2be48f2.zip"],  # 2022-08-09
)

http_archive(
    name = "com_google_googletest",
    sha256 = "24564e3b712d3eb30ac9a85d92f7d720f60cc0173730ac166f27dda7fed76cb2",
    strip_prefix = "googletest-release-1.12.1",
    urls = ["https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip"],
)

http_archive(
    name = "rules_cc",
    sha256 = "69fb4b965c538509324960817965791761d57010f42bf12ce9769c4259c7d018",
    strip_prefix = "rules_cc-e7c97c3af74e279a5db516a19f642e862ff58548",
    urls = ["https://github.com/bazelbuild/rules_cc/archive/e7c97c3af74e279a5db516a19f642e862ff58548.zip"],
)

# Google logging. Hopefully, this functionality makes it to absl so that we can drop this
# extra dependency.
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "cfdba0f2f17e8b1ff75c98113d5080d8ec016148426abcc19130864e2952d7bd",
    strip_prefix = "gflags-827c769e5fc98e0f2a34c47cef953cc6328abced",
    urls = ["https://github.com/gflags/gflags/archive/827c769e5fc98e0f2a34c47cef953cc6328abced.zip"],
)

http_archive(
    name = "com_github_google_glog",
    sha256 = "087a3de2eccce54a597fbb9d1530d4b8c1bae6ff6083511c19fe56b01a1f4f05",
    strip_prefix = "glog-0.5.0-rc2",
    urls = ["https://github.com/google/glog/archive/v0.5.0-rc2.tar.gz"],
)

#
# External tools needed
#

# 'make install' equivalent rule
http_archive(
    name = "com_github_google_rules_install",
    sha256 = "880217b21dbd40928bbe3bca3d97bd4de7d70d5383665ec007d7e1aac41d9739",
    strip_prefix = "bazel_rules_install-5ae7c2a8d22de2558098e3872fc7f3f7edc61fb4",
    urls = ["https://github.com/google/bazel_rules_install/archive/5ae7c2a8d22de2558098e3872fc7f3f7edc61fb4.zip"],
)

load("@com_github_google_rules_install//:deps.bzl", "install_rules_dependencies")

install_rules_dependencies()

load("@com_github_google_rules_install//:setup.bzl", "install_rules_setup")

install_rules_setup()

# Need to load before rules_flex/rules_bison to make sure
# win_flex_bison is the chosen toolchain on Windows
load("//bazel:win_flex_bison.bzl", "win_flex_configure")

win_flex_configure(
    name = "win_flex_bison",
    sha256 = "095cf65cb3f12ee5888022f93109acbe6264e5f18f6ffce0bda77feb31b65bd8",
    # bison 3.3.2, flex 2.6.4
    url = "https://github.com/lexxmark/winflexbison/releases/download/v2.5.18/win_flex_bison-2.5.18.zip",
)

http_archive(
    name = "rules_m4",
    sha256 = "b0309baacfd1b736ed82dc2bb27b0ec38455a31a3d5d20f8d05e831ebeef1a8e",
    urls = ["https://github.com/jmillikin/rules_m4/releases/download/v0.2.2/rules_m4-v0.2.2.tar.xz"],
)

load("@rules_m4//m4:m4.bzl", "m4_register_toolchains")

m4_register_toolchains()

http_archive(
    name = "rules_flex",
    sha256 = "f1685512937c2e33a7ebc4d5c6cf38ed282c2ce3b7a9c7c0b542db7e5db59d52",
    # flex 2.6.4
    urls = ["https://github.com/jmillikin/rules_flex/releases/download/v0.2/rules_flex-v0.2.tar.xz"],
)

load("@rules_flex//flex:flex.bzl", "flex_register_toolchains")

flex_register_toolchains()

http_archive(
    name = "rules_bison",
    sha256 = "9577455967bfcf52f9167274063ebb74696cb0fd576e4226e14ed23c5d67a693",
    urls = ["https://github.com/jmillikin/rules_bison/releases/download/v0.2.1/rules_bison-v0.2.1.tar.xz"],
)

load("@rules_bison//bison:bison.bzl", "bison_register_toolchains")

bison_register_toolchains()

http_archive(
    name = "com_google_protobuf",
    sha256 = "468a16f50694822291da57e304197f5322607dbed1a9d93192ff18de642c6cac",
    strip_prefix = "protobuf-21.5",
    urls = [
        "https://github.com/protocolbuffers/protobuf/archive/v21.5.zip",
    ],
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

http_archive(
    name = "rules_proto",
    sha256 = "e017528fd1c91c5a33f15493e3a398181a9e821a804eb7ff5acdd1d2d6c2b18d",
    strip_prefix = "rules_proto-4.0.0-3.20.0",
    urls = [
        "https://github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0-3.20.0.tar.gz",
    ],
)

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()

http_archive(
    name = "rules_python",
    sha256 = "778197e26c5fbeb07ac2a2c5ae405b30f6cb7ad1f5510ea6fdac03bded96cc6f",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_python/releases/download/0.2.0/rules_python-0.2.0.tar.gz",
        "https://github.com/bazelbuild/rules_python/releases/download/0.2.0/rules_python-0.2.0.tar.gz",
    ],
)

http_archive(
    name = "jsonhpp",
    build_file = "//bazel:jsonhpp.BUILD",
    sha256 = "081ed0f9f89805c2d96335c3acfa993b39a0a5b4b4cef7edb68dd2210a13458c",
    strip_prefix = "json-3.10.2",
    urls = [
        "https://github.com/nlohmann/json/archive/refs/tags/v3.10.2.tar.gz",
    ],
)

http_archive(
    name = "python_six",
    build_file = "//bazel:python_six.BUILD",
    sha256 = "30639c035cdb23534cd4aa2dd52c3bf48f06e5f4a941509c8bafd8ce11080259",
    strip_prefix = "six-1.15.0",
    urls = [
        "https://files.pythonhosted.org/packages/6b/34/415834bfdafca3c5f451532e8a8d9ba89a21c9743a0c59fbd0205c7f9426/six-1.15.0.tar.gz",
    ],
)

http_archive(
    name = "python_anytree",
    build_file = "//bazel:python_anytree.BUILD",
    sha256 = "79ee0cc74456950003287b0b5c7b76b7d09435563a31d9e553da484325043e1f",
    strip_prefix = "anytree-2.8.0",
    urls = [
        "https://github.com/c0fec0de/anytree/archive/2.8.0.tar.gz",
    ],
)

http_archive(
    name = "com_grail_bazel_compdb",
    sha256 = "f798690ddb6bba453ed489665c408bb0ce630bd7f0992c160c9414f933481a91",
    strip_prefix = "bazel-compilation-database-ace73b04e76111afa09934f8771a2798847e724e",
    urls = ["https://github.com/grailbio/bazel-compilation-database/archive/ace73b04e76111afa09934f8771a2798847e724e.tar.gz"],
)

git_repository(
    name = "boringssl",
    # Commits must come from the main-with-bazel branch.
    commit = "b9232f9e27e5668bc0414879dcdedb2a59ea75f2",
    # Use the GitHub mirror because the official source at
    # https://boringssl.googlesource.com/boringssl does not allow
    # unauthenticated git clone and the archives suffer from
    # https://github.com/google/gitiles/issues/84 preventing the use of
    # sha256sum on archives.
    remote = "https://github.com/google/boringssl",
    shallow_since = "1603819042 +0000",
)

http_archive(
    name = "absl_py",
    sha256 = "b9130d6f49a21dc44f56da89d5e8409807e93d28c194c23e27777f3c8cceef81",
    strip_prefix = "abseil-py-1.2.0",
    urls = ["https://github.com/abseil/abseil-py/archive/refs/tags/v1.2.0.tar.gz"],
)
