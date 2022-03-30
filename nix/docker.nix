{ pkgs ? import <nixpkgs> { }
, ...
}:

with pkgs;
let
  dependencies = import ./dependencies.nix {
    inherit pkgs;
    enableUnitTests = true;
    enableB2bua = true;
  };
in

dockerTools.buildLayeredImage {
  name = "gitlab.linphone.org:4567/bc/public/flexisip/bc-dev-nix-full";
  tag = "20220323_init";

  contents = dependencies ++ [
    # For Gitlab's CI script
    busybox # Without it, the image wouldn't even have `sh`
    ccache

    # TODO: With gcc, ld fails to find libX11
    ninja
    clang

    # Automatically pulled when using nix-shell, but have to be listed explicitely here
    openssl.dev
    openssl.out
    postgresql.lib
    zlib.dev
    sqlite.dev
    sqlite.out
    srtp.dev
    speex.dev
    speexdsp.dev
    speexdsp.out
    ffmpeg.dev
    ffmpeg.out
    xorg.libX11.dev
    xorg.xorgproto
    libxml2.dev
    libxml2.out
    nghttp2.dev
    nghttp2.lib
    net-snmp.dev
    libmysqlclient.dev
    libmysqlclient.out
    jsoncpp.dev

    # Unit tests
    boost.dev
  ];

  config = {
    Cmd = [ "/bin/sh" ];
    Env = [
      "CMAKE_INCLUDE_PATH=/include:/include/libxml2"
      "CMAKE_LIBRARY_PATH=/lib"
      "CC=clang"
      "CXX=clang++"
      "LD_LIBRARY_PATH=/lib/mariadb"
    ];
  };
}
