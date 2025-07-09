{
  description = "cclock";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default";
  };

  outputs = { self, nixpkgs, systems, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      dependencies = with pkgs; [
        clang
        gtk4.dev
        gtk4-layer-shell
        cmake
        ninja
        pkg-config
        sysprof
      ];

      devDependencies = dependencies ++ (with pkgs; [
        gdb
      ]);
    in
    {
      packages = {
        cclock = pkgs.stdenv.mkDerivation {
          pname = "cclock";
          version = "0.1";
          src = ./.;
          buildInputs = dependencies;

          installPhase = ''
            mkdir -p $out/bin
            cp cclock $out/bin/cclock
          '';
        };

        default = self.packages.${system}.cclock;
      };

      devShells.${system} = {
        default = pkgs.mkShell {
          buildInputs = devDependencies;
        };
      };

      formatter = pkgs.nixfmt-rfc-style;
    };
}
