workspace(name = "com_google_verible")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

#
# External tools needed
#

# 'make install' equivalent rule 2023-02-21
http_archive(
    name = "com_github_google_rules_install",
    # The installer uses an option -T that is not available on MacOS, but
    # it is benign to leave out.
    # Upstream bug https://github.com/google/bazel_rules_install/issues/31
    patch_args = ["-p1"],
    patches = ["//bazel:installer.patch"],
    sha256 = "aba3c1ae179beb92c1fc4502d66d7d7c648f90eb51897aa4b0ae4a76ce225eec",
    strip_prefix = "bazel_rules_install-6001facc1a96bafed0e414a529b11c1819f0cdbe",
    urls = ["https://github.com/google/bazel_rules_install/archive/6001facc1a96bafed0e414a529b11c1819f0cdbe.zip"],
)

load("@com_github_google_rules_install//:deps.bzl", "install_rules_dependencies")

install_rules_dependencies()

load("@com_github_google_rules_install//:setup.bzl", "install_rules_setup")

install_rules_setup()

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

# 2024-02-06
http_archive(
    name = "rules_compdb",
    sha256 = "70232adda61e89a4192be43b4719d35316ed7159466d0ab4f3da0ecb1fbf00b2",
    strip_prefix = "bazel-compilation-database-fa872dd80742b3dccd79a711f52f286cbde33676",
    urls = ["https://github.com/grailbio/bazel-compilation-database/archive/fa872dd80742b3dccd79a711f52f286cbde33676.tar.gz"],
)

load("@rules_compdb//:deps.bzl", "rules_compdb_deps")

rules_compdb_deps()
