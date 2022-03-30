nix-build docker.nix
docker load < result
rm result
