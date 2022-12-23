workspace(name = "com_google_verible")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

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
    sha256 = "13c086356d01079377b9d894ee461cf63550bafd4119e93107afa70e0eb16fec",
    strip_prefix = "abseil-cpp-0a066f31d981d69f7bde961055691906dabd4a3c",
    urls = ["https://github.com/abseil/abseil-cpp/archive/0a066f31d981d69f7bde961055691906dabd4a3c.zip"],
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
    sha256 = "8d324b62be33604b2c45ad1dd34ab93d722534448f55a16ca7292de32b6ac135",
    # bison 3.8.2, flex 2.6.4
    url = "https://github.com/lexxmark/winflexbison/releases/download/v2.5.25/win_flex_bison-2.5.25.zip",
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

# A version slightly beyond 21.5 as it fixes a warning
http_archive(
    name = "com_google_protobuf",
    sha256 = "efcd93bb1a228d08b4b0fcda00e710592fdf17ce6d0f5c8a239c2db0ba268a3f",
    strip_prefix = "protobuf-4efbcc44605731dc31507b94f0b81d2bd5ec169b",
    urls = [
        "https://github.com/protocolbuffers/protobuf/archive/4efbcc44605731dc31507b94f0b81d2bd5ec169b.zip",
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
    sha256 = "a3ff6fe238eec8202270dff75580cba3d604edafb8c3408711e82633c153efa8",
    strip_prefix = "bazel-compilation-database-940cedacdb8a1acbce42093bf67f3a5ca8b265f7",
    urls = ["https://github.com/grailbio/bazel-compilation-database/archive/940cedacdb8a1acbce42093bf67f3a5ca8b265f7.tar.gz"],
)

# zlib is imported through protobuf. Make the dependency explicit considering
# it's used outside protobuf.
maybe(
    http_archive,
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    sha256 = "91844808532e5ce316b3c010929493c0244f3d37593afd6de04f71821d5136d9",
    strip_prefix = "zlib-1.2.12",
    urls = [
        "https://zlib.net/zlib-1.2.12.tar.gz",
        "https://zlib.net/fossils/zlib-1.2.12.tar.gz",
    ],
)

# For sha256; Version taken from branch with bazel rules
# https://github.com/google/boringssl/tree/master-with-bazel
http_archive(
    name = "com_google_boringssl",
    sha256 = "482796f369c8655dbda3be801ae98c47916ecd3bff223d007a723fd5f5ecba22",
    strip_prefix = "boringssl-d345d68d5c4b5471290ebe13f090f1fd5b7e8f58",
    urls = ["https://github.com/google/boringssl/archive/d345d68d5c4b5471290ebe13f090f1fd5b7e8f58.zip"],
)

# Toolchain that we use for sanitizer runs in the CI
http_archive(
    name = "com_grail_bazel_toolchain",
    sha256 = "77c368349e1a90e6e133c7a16743c622856a0e6b4c9935d7d40ece53cf9d3576",
    strip_prefix = "bazel-toolchain-7e7c7cf1f965f348861085183d79b6a241764390",
    urls = ["https://github.com/grailbio/bazel-toolchain/archive/7e7c7cf1f965f348861085183d79b6a241764390.tar.gz"],
)

load("@com_grail_bazel_toolchain//toolchain:deps.bzl", "bazel_toolchain_dependencies")

bazel_toolchain_dependencies()

load("@com_grail_bazel_toolchain//toolchain:rules.bzl", "llvm_toolchain")

llvm_toolchain(
    name = "sanitizer_toolchain",
    llvm_version = "10.0.1",
    sha256 = {
        "linux": "02a73cfa031dfe073ba8d6c608baf795aa2ddc78eed1b3e08f3739b803545046",
    },
    strip_prefix = {
        "linux": "clang+llvm-10.0.1-x86_64-pc-linux-gnu",
    },
    urls = {
        "linux": [
            # Use a custom built Clang+LLVM binrary distribution that is more portable than
            # the official builds because it's built against an older glibc and does not have
            # dynamic library dependencies to tinfo, gcc_s or stdlibc++.
            #
            # For more details, see the files under toolchains/clang.
            "https://github.com/retone/deps/releases/download/na5/clang+llvm-10.0.1-x86_64-pc-linux-gnu.tar.xz",
        ],
    },
)
