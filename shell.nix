# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
let
  verible_used_stdenv = pkgs.stdenv;

  # Alternatively, use ccache stddev, so after bazel clean
  # it is much cheaper to rebuild the world.
  #
  # This requires that you add a line to your ~/.bazelrc
  # echo "build --sandbox_writable_path=$HOME/.cache/ccache" >> ~/.bazelrc
  # Works on nixos, but noticed issues with just nix package manager.
  #verible_used_stdenv = pkgs.ccacheStdenv;

  # Testing with specific compilers
  #verible_used_stdenv = pkgs.gcc13Stdenv;

  # Using clang stdenv does not work yet out of the box yet
  # https://github.com/NixOS/nixpkgs/issues/216047
  #verible_used_stdenv = pkgs.clang13Stdenv;
in
verible_used_stdenv.mkDerivation {
  name = "verible-build-environment";
  buildInputs = with pkgs;
    [
      bazel_4
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
      clang-tools_14    # clang-format, clang-tidy
      bazel-buildtools  # buildifier
    ];
}
