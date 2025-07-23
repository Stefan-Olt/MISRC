# MISRC tools


## misrc_capture


Tool to capture, extract and compress data from the HDMI input of MS2130 using hsdaoh.


### Description


This tool will disable any procssing on the MS2130 and capture the data using hsdaoh.

A MISRC `1.x/2.x` (with Tang Nano 20k setup) will pack the data into an HDMI signal for the MS2130.

`misrc_capture` will unpack the data in realtime and can directly output two separate files for the ADCs and a file for the aux data.

For x86_64 (64 bit AMD and Intel processors) there is handwritten assembly for higher performance using SSE instructions.


# Usage


- Run tools without arguments to see get help on comamnd options


## misrc_capture 


A simple program to capture from MISRC using hsdaoh

Example:

    misrc_capture -p -f -l 8 -a video_rf.flac -b hifi_rf.flac -x baseband_audio.bin

Usage:

- `-d` device_index (default: 0) (select target MS21xx device for capture)
- `-n` number of samples to read (default: 0, infinite)
- `-t` time to capture (seconds, m:s or h:m:s; -n takes priority, assumes 40msps)
- `-w` overwrite any files without asking
- `-a` ADC A output file (use '-' to write on stdout)  
- `-b` ADC B output file (use '-' to write on stdout)  
- `-x` AUX output file (use '-' to write on stdout)  
- `-r` RAW 32-Bit data output file (use '-' to write on stdout)  
- `-p` pad lower 4 bits of 16 bit output with 0 instead of upper 4
- `-A` suppress clipping messages for ADC A (need to specify -a or -r as well)
- `-B` suppress clipping messages for ADC B (need to specify -a or -r as well)
- `-f` compress ADC output as FLAC  
- `-l` LEVEL set flac compression level (default: 1) 
- `-v` enable verification of flac encoder output  
- `-c` number of flac encoding threads per file (default: auto)


## misrc_extract


The MISRC has 2 ADC channels with 12-bit's each and the 8-bit's of AUX data is also interleaved into a total of 32 bit words.

This tool will manually deinterleave the data for x86_64 (64 bit AMD and Intel processors).


## Extract


A simple program for extracting captured raw data into separate files

Example:

    misrc_extract -i raw_32-bit_dump.bin -p -a channel_1.s16 -b channel_2.s16 -x aux_data.bin

Usage:

- `-i` Input file (use '-' to read from stdin)  
- `-a` ADC A output file (use '-' to write on stdout)  
- `-b` ADC B output file (use '-' to write on stdout)  
- `-x` AUX output file (use '-' to write on stdout)  
- `-p` pad lower 4 bits of 16 bit output with 0 instead of upper 4  
- `-s` input is captured as single channel (-b cannot be used)  


## Version History


* 0.4
    * Release of new tool: misrc_capture for direct capture from HDMI/MS2130 using hsdaoh
    * Builds for Linux, macOS and Windows (x86_64 and arm64 on all platforms)

* 0.3.1
    * Fix: incorrect align could lead to crash (fix by namazso)

* 0.3
    * New: allow extraction of data from single channel capture
      (Cypress FX3 setup)

* 0.2
    * Fix: signal now with correct polarity
    * Mod: output program name and version
    * New: option to pad lower 4 bits instead of upper 4

* 0.1
    * Initial Release


## Building / Compilation


### Required build tools


- A somewhat modern C compiler
- git (for manual compilation)
- curl (for automatic build script)
- cmake
- nasm (for x86_64 platform)
- meson/ninja (for static builds/automatic compilation)

Installation of build tools:
- MacOS using [brew](https://brew.sh/): 

      brew install git curl cmake nasm meson ninja

- Linux (Debian, Ubuntu, Mint) using apt: 

      apt install build-essential git curl cmake nasm meson ninja-build

- Windows (msys2/UCRT64) using pacman: 

      pacman -S git curl mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkg-config mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-nasm mingw-w64-ucrt-x86_64-meson mingw-w64-ucrt-x86_64-ninja


### Automatic compilation


The script `build/build-static.sh` can be used for bulding a statically linked version.
It will download and compile dependencies. It is used to build the binaries downloadable here.
In case it doesn't work for your needs you can compile misrc_tools manually.


### Dependencies


- [libusb](https://github.com/libusb/libusb)
- [libuvc](https://github.com/libuvc/libuvc) (for Windows cmake fixes use [Steve-m's fork](https://github.com/steve-m/libuvc))
- [hsdaoh](https://github.com/Stefan-Olt/hsdaoh) (the main hsdaoh branch (steve-m) does not have required changes merged, use the linked one!)
- [FLAC](https://github.com/xiph/flac) (v1.5.0 and newer required for multi-threading support!)

Installation of FLAC, libusb and libuvc (if available):

- macOS using [brew](https://brew.sh/): 

      brew install flac libusb

- Linux (Debian, Ubuntu, Mint) using apt: 

      apt install libflac-dev libusb-1.0-0-dev libuvc-dev

- Windows (msys2/UCRT64) using pacman: 

      pacman -S mingw-w64-ucrt-x86_64-flac mingw-w64-ucrt-x86_64-libusb


## Manual Build Process


1. Build libuvc (required on Windows and macOS):
<details closed>
<summary>macOS (Linux, BSD...)</summary>

```
git clone https://github.com/libuvc/libuvc.git
mkdir libuvc/build && cd libuvc/build
cmake ../
make
sudo make install
```


</details>
<details closed>
<summary>Windows</summary>


```
git clone https://github.com/steve-m/libuvc.git
mkdir libuvc/build && cd libuvc/build
cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=/mingw64
cmake --build .
cmake --install .
```
</details>

2. Build hsdaoh
<details closed>
<summary>MacOS (Linux, BSD...)</summary>


```
git clone https://github.com/Stefan-Olt/hsdaoh.git
mkdir hsdaoh/build
cd hsdaoh/build
cmake ../ -DINSTALL_UDEV_RULES=ON
make
sudo make install
sudo ldconfig
```


</details>
<details closed>
<summary>Windows</summary>


```
git clone https://github.com/Stefan-Olt/hsdaoh.git
mkdir hsdaoh/build && cd hsdaoh/build
cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=/mingw64
cmake --build .
cmake --install .
```
</details>

3. Build misrc_tools

<details closed>
<summary>MacOS (Linux, BSD...)</summary>

```
git clone https://github.com/Stefan-Olt/MISRC.git
cd ./MISRC/misrc_tools
mkdir build
cd build
cmake ../
make
sudo make install
```

</details>
<details closed>
<summary>Windows</summary>

```
git clone https://github.com/Stefan-Olt/MISRC.git
cd ./MISRC/misrc_tools
mkdir build
cd build
cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=/mingw64
cmake --build .
cmake --install .
```
</details>


## License


This project is licensed under the GNU General Public License v3 or later - see the LICENSE file for details.
