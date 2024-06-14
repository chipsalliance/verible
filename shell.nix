# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
let
  verible_used_stdenv = pkgs.stdenv;
  #verible_used_stdenv = pkgs.gcc13Stdenv;
  #verible_used_stdenv = pkgs.clang17Stdenv;
in
verible_used_stdenv.mkDerivation {
  name = "verible-build-environment";
  buildInputs = with pkgs;
    [
      bazel_6
      jdk11
      git

      # For scripts used inside bzl rules and tests
      gnused
      python3

      # To run error-log-analyzer
      python3Packages.mdutils
      ripgrep

      # To manually run export_json_examples
      python3Packages.anytree

      # For using --//bazel:use_local_flex_bison if desired
      flex
      bison

      # To build vscode vsix package
      nodejs

      # Ease development
      lcov              # coverage html generation.
      bazel-buildtools  # buildifier

      clang-tools_17    # For clang-tidy; clangd
    ];
}
