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
      bazel_5
      jdk11
      git

      # For scripts used inside bzl rules and tests
      gnused
      python3

      # To run error-log-analyzer
      python3Packages.mdutils
      ripgrep

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

  shellHook = ''
      # We choose the last clang-format that produces the same result
      # as the one used on the github CI (newer than v15 arrange some things
      # slightly differently, so would result in a conflict).
      export CLANG_FORMAT=${pkgs.clang-tools_15}/bin/clang-format
  '';
}
