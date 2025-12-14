{
  description = "No effort libvibrant-like plugin for Hyprland";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    hyprland.url = "github:hyprwm/Hyprland";
  };

  outputs =
    {
      self,
      nixpkgs,
      ...
    }@inputs:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      eachSystem = nixpkgs.lib.genAttrs systems;
      pkgsFor = nixpkgs.legacyPackages;
    in
    {
      checks = eachSystem (system: {
        build = self.packages.${system}.hyprvibr;
      });

      packages = eachSystem (system: {
        default = self.packages.${system}.hyprvibr;
        hyprvibr =
          let
            inherit (inputs.hyprland.packages.${system}) hyprland;
            inherit (pkgsFor.${system}) stdenvNoCC gcc14;

            name = "hyprvibr";
          in
          stdenvNoCC.mkDerivation {
            inherit name;
            pname = name;
            src = ./.;

            inherit (hyprland) buildInputs;
            nativeBuildInputs = hyprland.nativeBuildInputs ++ [
              hyprland
              gcc14
            ];
            enableParallelBuilding = true;

            dontUseCmakeConfigure = true;
            dontUseMesonConfigure = true;
            dontUseNinjaBuild = true;
            dontUseNinjaInstall = true;

            installPhase = ''
              runHook preInstall

              mkdir -p "$out/lib"
              cp out/hyprvibr.so "$out/lib/lib${name}.so"

              runHook postInstall
            '';
          };
      });
    };
}
