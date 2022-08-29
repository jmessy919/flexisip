args@{ pkgs ? import <nixpkgs> { }, ... }:

import ./nix/base.nix ({
  inherit pkgs;
  enableUnitTests = true;
  enableB2bua = true;
  additionalInputs = ps: with ps; [
    nixpkgs-fmt
    ccache
    clang_13
  ];
} // args)
