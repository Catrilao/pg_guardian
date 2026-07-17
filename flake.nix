{
  description = "PostgreSQL Extension Dev Enviroment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
    pg = pkgs.postgresql_18;
  in {

    packages.${system}.default = pkgs.stdenv.mkDerivation {
      pname = "pg_guardian";
      version = "1.0";

      src = ./.;

      nativeBuildInputs = [
        pkgs.gnumake
        pg.pg_config
      ];

      buildPhase = ''
        make
      '';

      installPhase = ''
        make install \
        prefix=$out \
        pkglibdir=$out/lib \
        datadir=$out/share
      '';
    };

    devShells.${system}.default = pkgs.mkShell {
      nativeBuildInputs = builtins.attrValues {
        inherit
          (pkgs)
          nixd
          alejandra
          gcc
          clang
          clang-tools
          gnumake
          bear
          pkg-config
          cargo
          rustc
          cargo-pgrx
          rustfmt
          ;
        inherit (pg) pg_config;
      };

      buildInputs = builtins.attrValues {
        inherit (pkgs) openssl;
        inherit pg;
      };

      PG_CONFIG = "${pg.pg_config}/bin/pg_config";
      PGRX_PG_CONFIG_PATH = "${pg.pg_config}/bin/pg_config";
    };
  };
}
