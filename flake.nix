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
        # clang
        zig
        gtk4.dev
        gtk4-layer-shell
        # cmake
        # ninja
        pkg-config
        # sysprof
      ];

      devDependencies = dependencies ++ (with pkgs; [
        # gdb
      ]);
    in
    {
      packages.${system} = {
        cclock = pkgs.stdenv.mkDerivation {
          pname = "cclock";
          version = "0.1";
          src = ./.;
          nativeBuildInputs = [ pkgs.zig pkgs.pkg-config ];
          buildInputs = with pkgs; [
            gtk4.dev
            gtk4-layer-shell
          ];

          buildPhase = ''
            export HOME=$TMPDIR
            zig build -Doptimize=ReleaseFast
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp zig-out/bin/cclock $out/bin/cclock

            mkdir -p $out/share/applications
            cat > $out/share/applications/cclock.desktop <<EOF
[Desktop Entry]
Name=CClock
Comment=Countdown timer overlay
TryExec=cclock
Exec=cclock --picker
Terminal=false
Type=Application
Categories=Utility;
StartupNotify=false
EOF
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
