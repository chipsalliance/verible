# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      bash
      bazel_4
      gcc
      git
      glibc
      jdk11
      lcov
      python3

      # for experimenting with --//bazel:use_local_flex_bison
      flex
      bison

      # To build vscode vsix package
      nodejs

      # Ease development
      clang-tools_11    # clang-format
      bazel-buildtools  # buildifier
    ];
}
