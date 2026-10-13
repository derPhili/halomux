{
  description = "openjowelsofts/huenicorn, packaged for usage on Nixos";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
    httplib = {
      url = "github:yhirose/cpp-httplib?ref=v0.46.0";
      flake = false;
    };
  };

  outputs =
    {
      flake-utils,
      nixpkgs,
      httplib,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = (import nixpkgs) {
          inherit system;

          # to allow for rust-rover to be installed
          config.allowUnfree = true;
        };
        
        newerHttpLib = pkgs.httplib.overrideAttrs (old: {
            version = "0.46.0";
            src = httplib;
        });

        buildDependencies = with pkgs; [
          curl
          gcc
          glib
          glm
          gnumake
          newerHttpLib
          mbedtls
          nlohmann_json
          opencv
          pkg-config
          libX11
          libXcursor
          libXrandr
          libXi
          cmake
          pipewire
        ];

        built = pkgs.stdenv.mkDerivation {
          name = "huenicorn";

          nativeBuildInputs = buildDependencies;

          src = ./.;

          buildPhase = ''
            cp -r $src ./src
            chmod +w src
            mkdir -p ./src/build
            cd ./src/build
            cmake ..
            make
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp huenicorn $out/bin/
          '';
        };
      in
      {
        packages = {
          default = built;
        };

        checks = {
          huenicorn-version-works = pkgs.stdenv.mkDerivation {
            name = "Huenicorn `--version` smoke test";

            src = ./.;

            dontBuild = true;
            doCheck = true;

            checkPhase = ''
               ${built}/bin/huenicorn --version | ${pkgs.gnugrep}/bin/grep Huenicorn >> $out;
            '';
          };
        };
      }
    );
}
