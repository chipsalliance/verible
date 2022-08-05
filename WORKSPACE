workspace(name = "com_google_verible")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "com_google_absl",
    # On MSVC's STL implementation, string_view cannot be constructed from
    # a string_view::iterator. This patch forces the use of absl's string_view
    # implementation to solve the issue
    patch_args = ["-p1"],
    patches = ["//bazel:absl.patch"],
    sha256 = "a4567ff02faca671b95e31d315bab18b42b6c6f1a60e91c6ea84e5a2142112c2",
    strip_prefix = "abseil-cpp-20211102.0",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20211102.0.zip"],
)

# Googletest
http_archive(
    name = "com_google_googletest",
    sha256 = "353571c2440176ded91c2de6d6cd88ddd41401d14692ec1f99e35d013feda55a",
    strip_prefix = "googletest-release-1.11.0",
    urls = ["https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip"],
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
    urls = ["https://github.com/gflags/gflags/archive/827c769e5fc98e0f2a34c47cef953cc6328abced.zip" ],
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
    sha256 = "9147455f1c234fba7922731fb32842f6b3ad01dcafe344be5292f80f82b25dad",
    strip_prefix = "bazel_rules_install-4cd8ab0b5d8a0117bb5b8c89a0024508d5d4d5ed",
    urls = ["https://github.com/google/bazel_rules_install/archive/4cd8ab0b5d8a0117bb5b8c89a0024508d5d4d5ed.zip"],
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
    sha256 = "c67fa9891bb19e9e6c1050003ba648d35383b8cb3c9572f397ad24040fb7f0eb",
    # m4 1.4.18
    urls = ["https://github.com/jmillikin/rules_m4/releases/download/v0.2/rules_m4-v0.2.tar.xz"],
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
    sha256 = "6ee9b396f450ca9753c3283944f9a6015b61227f8386893fb59d593455141481",
    # bison 3.3.2
    urls = ["https://github.com/jmillikin/rules_bison/releases/download/v0.2/rules_bison-v0.2.tar.xz"],
)

load("@rules_bison//bison:bison.bzl", "bison_register_toolchains")

bison_register_toolchains()

# We have to import zlib directly ourselves, because protobuf_deps.bzl isn't
# part of the protobuf release yet
# (https://github.com/protocolbuffers/protobuf/issues/5918).
http_archive(
    name = "net_zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    sha256 = "91844808532e5ce316b3c010929493c0244f3d37593afd6de04f71821d5136d9",
    strip_prefix = "zlib-1.2.12",
    urls = [
        "https://zlib.net/zlib-1.2.12.tar.gz",
        "https://zlib.net/fossils/zlib-1.2.12.tar.gz",
    ],
)

http_archive(
    name = "com_google_protobuf",
    repo_mapping = {"@zlib": "@net_zlib"},
    sha256 = "1c744a6a1f2c901e68c5521bc275e22bdc66256eeb605c2781923365b7087e5f",
    strip_prefix = "protobuf-3.13.0",
    urls = [
        "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/archive/v3.13.0.zip",
        "https://github.com/protocolbuffers/protobuf/archive/v3.13.0.zip",
    ],
)

http_archive(
    name = "rules_proto",
    sha256 = "e4fe70af52135d2ee592a07f916e6e1fc7c94cf8786c15e8c0d0f08b1fe5ea16",
    strip_prefix = "rules_proto-97d8af4dc474595af3900dd85cb3a29ad28cc313",
    url = "https://github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.zip",
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
    strip_prefix = "json-3.10.2",
    sha256 = "081ed0f9f89805c2d96335c3acfa993b39a0a5b4b4cef7edb68dd2210a13458c",
    urls = [
        "https://github.com/nlohmann/json/archive/refs/tags/v3.10.2.tar.gz"
    ]
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
