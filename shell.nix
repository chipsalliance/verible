# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      bazel_4
      jdk11
      git

      # For scripts used inside bzl rules and tests
      gnused
      python3

      # For using --//bazel:use_local_flex_bison if desired
      flex
      bison

      # To build vscode vsix package
      nodejs

      # Ease development
      lcov              # coverage html generation.
      clang-tools_14    # clang-format, clang-tidy
      bazel-buildtools  # buildifier
    ];
}
