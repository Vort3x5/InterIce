{ pkgs ? import <nixpkgs> {} }:
let
  kernel = pkgs.linuxPackages.kernel;
in
pkgs.mkShell {
  nativeBuildInputs = kernel.moduleBuildDependencies ++ [ pkgs.gnumake ];
  KDIR = "${kernel.dev}/lib/modules/${kernel.modDirVersion}/build";
}
