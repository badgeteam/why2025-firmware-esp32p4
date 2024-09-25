with import <nixpkgs> {};
mkShell {
  buildInputs = [
    python3
    python3Packages.pip
    python3Packages.virtualenv
    libusb
    udev
    libz

    screen
    gnumake
    cmake
    pkg-config
    ninja
    wget
    ncurses5

  ];

  
  NIX_LD_LIBRARY_PATH = lib.makeLibraryPath [
    libusb
    udev
    libz
  ];

  NIX_LD = builtins.readFile "${stdenv.cc}/nix-support/dynamic-linker";

  shellHook = ''
    export PORT=/dev/ttyUSB0
    export IDF_PATH=$(echo `pwd`/esp-idf) 
    export PATH=$PATH:$IDF_PATH/tools
    
    alias monitor="sudo chmod 666 /dev/ttyUSB0 && screen /dev/ttyUSB0 115200"
    alias flashmon="sudo chmod 666 /dev/ttyUSB0 && make flash && screen /dev/ttyUSB0 115200"
  '';

}
