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
      bazel_7
      git

      # For scripts used inside bzl rules and tests
      gnused

      # To run error-log-analyzer
      python3
      python3Packages.mdutils
      ripgrep

      # To manually run export_json_examples
      python3Packages.anytree

      # To build vscode vsix package
      nodejs

      # Ease development
      lcov              # coverage html generation.
      bazel-buildtools  # buildifier

      llvmPackages_19.clang-tools    # for clang-tidy
      llvmPackages_17.clang-tools    # for clang-format
    ];
  shellHook = ''
      # clang tidy: use latest.
      export CLANG_TIDY=${pkgs.llvmPackages_19.clang-tools}/bin/clang-tidy

      # There is too much volatility between even micro-versions of
      # later clang-format. Let's use stable 17 for now.
      export CLANG_FORMAT=${pkgs.llvmPackages_17.clang-tools}/bin/clang-format
  '';
}
