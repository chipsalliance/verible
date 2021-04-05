workspace(name = "com_google_verible")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
    name = "com_google_absl",
    sha256 = "11d6eea257cc9322cc49924cf9584dbe61922bfffe3e7c42e2bce3abc1694a1a",
    strip_prefix = "abseil-cpp-20210324.0",
    urls = ["https://github.com/abseil/abseil-cpp/archive/20210324.0.zip"],
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

# Needed for Kythe
http_archive(
    name = "io_bazel_rules_go",
    sha256 = "ac03931e56c3b229c145f1a8b2a2ad3e8d8f1af57e43ef28a26123362a1e3c7e",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.24.4/rules_go-v0.24.4.tar.gz",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.24.4/rules_go-v0.24.4.tar.gz",
    ],
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains()

# Google logging. Hopefully, this functionality makes it to absl so that we can drop this
# extra dependency.
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
    strip_prefix = "gflags-2.2.2",
    urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
)

http_archive(
    name = "com_github_google_glog",
    # Using the same version as in kythe
    # https://github.com/kythe/kythe/blob/410f69c5bcb69fabcb78a5200b7631a1bffabd31/external.bzl#L155
    sha256 = "9b4867ab66c33c41e2672b5de7e3133d38411cdb75eeb0d2b72c88bb10375c71",
    strip_prefix = "glog-ba8a9f6952d04d1403b97df24e6836227751454e",
    urls = ["https://github.com/google/glog/archive/ba8a9f6952d04d1403b97df24e6836227751454e.zip"],
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
    sha256 = "4d6798f368e23b9bf8ddab53beb57518b1960bd57549cca50e1ac61f4beb810b",
    strip_prefix = "rules_foreign_cc-4d4acaa112ae646a21e3766182b21882ad9df921",
    # There are no releases yet, so retrieve particular git version.
    # This is from 2021-01-25
    urls = ["https://github.com/bazelbuild/rules_foreign_cc/archive/4d4acaa112ae646a21e3766182b21882ad9df921.zip"],
)

load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

# 'make install' equivalent rule
http_archive(
    name = "com_github_google_rules_install",
    sha256 = "ac2c9c53aa022a110273c0e510d191a4c04c6adafefa069a5eeaa16313edc9b9",
    strip_prefix = "bazel_rules_install-0.4",
    urls = ["https://github.com/google/bazel_rules_install/releases/download/0.4/bazel_rules_install-0.4.tar.gz"],
)

load("@com_github_google_rules_install//:deps.bzl", "install_rules_dependencies")

install_rules_dependencies()

load("@com_github_google_rules_install//:setup.bzl", "install_rules_setup")

install_rules_setup()

all_content = """filegroup(name = "all", srcs = glob(["**"]), visibility = ["//visibility:public"])"""

http_archive(
    name = "bison",
    build_file_content = all_content,
    sha256 = "e28ed3aad934de2d1df68be209ac0b454f7b6d3c3d6d01126e5cd2cbadba089a",
    strip_prefix = "bison-3.6.2",
    urls = ["https://ftp.gnu.org/gnu/bison/bison-3.6.2.tar.gz"],
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

http_archive(
    name = "bazel_toolchains",
    sha256 = "882fecfc88d3dc528f5c5681d95d730e213e39099abff2e637688a91a9619395",
    strip_prefix = "bazel-toolchains-3.4.0",
    urls = [
        "https://github.com/bazelbuild/bazel-toolchains/releases/download/3.4.0/bazel-toolchains-3.4.0.tar.gz",
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-toolchains/releases/download/3.4.0/bazel-toolchains-3.4.0.tar.gz",
    ],
)

# TODO(ikr): This is a huge dependency pulled in only for bash unit test
# framework. Find a smaller alternative that works both internally and
# externally.
http_archive(
    name = "io_bazel",
    sha256 = "4822ac0f365210932803d324d1b0e08dbd451242720017c5b7f70716c2e3e059",
    strip_prefix = "bazel-3.7.0",
    urls = [
        "https://github.com/bazelbuild/bazel/archive/3.7.0.tar.gz",
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

# TODO(ikr): Replace with a Kythe release once it moves beyond 0.48
git_repository(
    name = "io_kythe",
    # branch = "master",
    # commit = "bee43a5908ce99cb9cf5a2cd42dc2da2972707f8", # broke issue #625
    # the last working commit on "master" between 0.48 and 0.49:
    commit = "410f69c5bcb69fabcb78a5200b7631a1bffabd31",
    remote = "https://github.com/kythe/kythe",
)

load("@io_kythe//:setup.bzl", "kythe_rule_repositories")

kythe_rule_repositories()

load("@io_kythe//:external.bzl", "kythe_dependencies")

kythe_dependencies()

http_archive(
    name = "jsoncpp_git",
    sha256 = "77a402fb577b2e0e5d0bdc1cf9c65278915cdb25171e3452c68b6da8a561f8f0",
    build_file = "//bazel:jsoncpp.BUILD",
    strip_prefix = "jsoncpp-1.9.2",
    urls = [
        "https://github.com/open-source-parsers/jsoncpp/archive/1.9.2.tar.gz",
    ],
)

http_archive(
    name = "python_six",
    sha256 = "30639c035cdb23534cd4aa2dd52c3bf48f06e5f4a941509c8bafd8ce11080259",
    build_file = "//bazel:python_six.BUILD",
    strip_prefix = "six-1.15.0",
    urls = [
        "https://files.pythonhosted.org/packages/6b/34/415834bfdafca3c5f451532e8a8d9ba89a21c9743a0c59fbd0205c7f9426/six-1.15.0.tar.gz",
    ],
)

http_archive(
    name = "python_anytree",
    sha256 = "79ee0cc74456950003287b0b5c7b76b7d09435563a31d9e553da484325043e1f",
    build_file = "//bazel:python_anytree.BUILD",
    strip_prefix = "anytree-2.8.0",
    urls = [
        "https://github.com/c0fec0de/anytree/archive/2.8.0.tar.gz",
    ],
)
