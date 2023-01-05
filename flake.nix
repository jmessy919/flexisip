{
  description = "A very basic flake";

  outputs = { self, nixpkgs }:
    let pkgs = nixpkgs.legacyPackages.x86_64-linux; in
    {

      # `nix build '.?submodules=1'`
      defaultPackage.x86_64-linux = pkgs.gcc6Stdenv.mkDerivation {
        name = "flexisip";
        version = "1.0.12-dev_subdomain_router";
        src = ./.;
        configurePhase = ''
          python prepare.py -d $configureFlags
        '';
        configureFlags = [
          "-DENABLE_DOC=NO"
          "-DENABLE_REDIS=NO"
        ];
        buildInputs = with pkgs; [
          python
          cmake
          pkg-config
          automake
          autoconf
          libtool
          openssl
        ];
        enableParallelBuilding = true;
        doCheck = true;
        checkPhase = ''
          OUTPUT/bin/flexisip --version
        '';
        installPhase = ''
          mkdir -p $out/bin
          # TODO
        '';
      };

      devShells.x86_64-linux.default = pkgs.mkShell {
        packages = with pkgs; [
          nixpkgs-fmt
        ];
      };

    };
}
