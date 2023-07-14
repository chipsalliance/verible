workspace(name = "com_google_verible")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

http_archive(
    name = "rules_license",
    sha256 = "6157e1e68378532d0241ecd15d3c45f6e5cfd98fc10846045509fb2a7cc9e381",
    urls = [
        "https://github.com/bazelbuild/rules_license/releases/download/0.0.4/rules_license-0.0.4.tar.gz",
        "https://mirror.bazel.build/github.com/bazelbuild/rules_license/releases/download/0.0.4/rules_license-0.0.4.tar.gz",
    ],
)

# Bazel platform rules, needed as dependency to absl.
http_archive(
    name = "platforms",
    sha256 = "5308fc1d8865406a49427ba24a9ab53087f17f5266a7aabbfc28823f3916e1ca",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
        "https://github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
    ],
)

http_archive(
    name = "bazel_skylib",
    sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
    ],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

http_archive(
    name = "com_google_absl",
    # On MSVC's STL implementation, string_view cannot be constructed from
    # a string_view::iterator. This patch forces the use of absl's string_view
    # implementation to solve the issue
    patch_args = ["-p1"],
    patches = [
        "//bazel:absl.patch",
        "//bazel:fix-uninitialized-var.patch",
    ],
    sha256 = "51d676b6846440210da48899e4df618a357e6e44ecde7106f1e44ea16ae8adc7",
    strip_prefix = "abseil-cpp-20230125.3",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20230125.3.zip"],
)

http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "8b4a8175da7205df2ad02e405a950a02eaa3e3e0840947cd598e92dca453199b",
    strip_prefix = "re2-2023-06-01",
    urls = ["https://github.com/google/re2/archive/refs/tags/2023-06-01.tar.gz"],
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
    # The installer uses an option -T that is not available on MacOS, but
    # it is benign to leave out.
    # Upstream bug https://github.com/google/bazel_rules_install/issues/31
    patch_args = ["-p1"],
    patches = ["//bazel:installer.patch"],
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
    sha256 = "10ce41f150ccfbfddc9d2394ee680eb984dc8a3dfea613afd013cfb22ea7445c",
    urls = ["https://github.com/jmillikin/rules_m4/releases/download/v0.2.3/rules_m4-v0.2.3.tar.xz"],
)

load("@rules_m4//m4:m4.bzl", "m4_register_toolchains")

m4_register_toolchains(version = "1.4.18")

http_archive(
    name = "rules_flex",
    sha256 = "8929fedc40909d19a4b42548d0785f796c7677dcef8b5d1600b415e5a4a7749f",
    # flex 2.6.4
    urls = ["https://github.com/jmillikin/rules_flex/releases/download/v0.2.1/rules_flex-v0.2.1.tar.xz"],
)

load("@rules_flex//flex:flex.bzl", "flex_register_toolchains")

flex_register_toolchains(version = "2.6.4")

http_archive(
    name = "rules_bison",
    sha256 = "9577455967bfcf52f9167274063ebb74696cb0fd576e4226e14ed23c5d67a693",
    urls = ["https://github.com/jmillikin/rules_bison/releases/download/v0.2.1/rules_bison-v0.2.1.tar.xz"],
)

load("@rules_bison//bison:bison.bzl", "bison_register_toolchains")

bison_register_toolchains()

# Slightly after 23.4, with fixed #13062 signedness warning.
http_archive(
    name = "com_google_protobuf",
    sha256 = "3023165dbf01e6e3a215124de6f932ae9ccc8c2e0b9d7e30e3023bba8ca08205",
    strip_prefix = "protobuf-5789e0ffb5af9cda52a72cf9d8d1b3acc596588d",
    urls = [
        "https://github.com/protocolbuffers/protobuf/archive/5789e0ffb5af9cda52a72cf9d8d1b3acc596588d.zip",
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

# 2022-09-19
http_archive(
    name = "com_grail_bazel_compdb",
    sha256 = "a3ff6fe238eec8202270dff75580cba3d604edafb8c3408711e82633c153efa8",
    strip_prefix = "bazel-compilation-database-940cedacdb8a1acbce42093bf67f3a5ca8b265f7",
    urls = ["https://github.com/grailbio/bazel-compilation-database/archive/940cedacdb8a1acbce42093bf67f3a5ca8b265f7.tar.gz"],
)

load("@com_grail_bazel_compdb//:deps.bzl", "bazel_compdb_deps")

bazel_compdb_deps()

# zlib is imported through protobuf. Make the dependency explicit considering
# it's used outside protobuf.
maybe(
    http_archive,
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    sha256 = "b3a24de97a8fdbc835b9833169501030b8977031bcb54b3b3ac13740f846ab30",
    strip_prefix = "zlib-1.2.13",
    urls = [
        "https://zlib.net/zlib-1.2.13.tar.gz",
        "https://zlib.net/fossils/zlib-1.2.13.tar.gz",
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
