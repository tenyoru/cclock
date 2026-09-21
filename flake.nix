{
  description = "cclock";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs =
    { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      qt = pkgs.qt6;
      cclock = pkgs.stdenv.mkDerivation {
        pname = "cclock";
        version = "0.4.0";
        meta = {
          description = "Countdown timer overlay";
          mainProgram = "cclock";
        };
        src = pkgs.lib.cleanSourceWith {
          src = ./.;
          filter =
            path: type:
            let
              name = baseNameOf path;
            in
            pkgs.lib.cleanSourceFilter path type
            && !(builtins.elem name [
              "build"
              "result"
              ".cache"
            ]);
        };

        nativeBuildInputs = [
          pkgs.cmake
          pkgs.ninja
          pkgs.pkg-config
          qt.wrapQtAppsHook
          qt.qtdeclarative
        ];

        buildInputs = [
          qt.qtbase
          qt.qtdeclarative
          qt.qtwayland
          pkgs.kdePackages.layer-shell-qt
        ];

        postInstall = ''
                    mkdir -p $out/share/applications
                    cat > $out/share/applications/cclock.desktop <<EOF
          [Desktop Entry]
          Name=CClock
          Comment=Countdown timer overlay
          Icon=cclock
          TryExec=cclock
          Exec=cclock --picker
          Terminal=false
          Type=Application
          Categories=Utility;
          StartupNotify=false
          EOF
        '';
      };
    in
    {
      packages.${system} = {
        inherit cclock;
        default = cclock;
      };

      apps.${system}.default = {
        type = "app";
        program = "${cclock}/bin/cclock";
        meta.description = "Countdown timer overlay";
      };

      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ cclock ];
        # wrapQtAppsHook only wraps the installed binary, so an in-tree build
        # finds no QML modules without this.
        QML_IMPORT_PATH = pkgs.lib.makeSearchPath "lib/qt-6/qml" [
          pkgs.qt6.qtdeclarative
          pkgs.kdePackages.layer-shell-qt
        ];
      };

      formatter.${system} = pkgs.nixfmt;
    };
}
