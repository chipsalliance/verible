# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{pkgs ? import (builtins.fetchTarball {
  # Descriptive name to make the store path easier to identify
  name = "nixos-2021-11";
  # Commit hash
  url = "https://github.com/NixOS/nixpkgs/archive/refs/tags/21.11.tar.gz";
  # Hash obtained using `nix-prefetch-url --unpack <url>`
  sha256 = "162dywda2dvfj1248afxc45kcrg83appjd0nmdb541hl7rnncf02";
}) {}}:


pkgs.mkShell {
  buildInputs = with pkgs;
    [
      bash
      bazel
      gcc
      git
      glibc
      jdk
      python3
    ];

}
