{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    yosys
    nextpnr
    icestorm
    openfpgaloader

    python3
    python3Packages.pyftdi
  ];

  shellHook = ''
    fish
  '';
}
