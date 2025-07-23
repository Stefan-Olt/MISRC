# MISRC - Multi Input Simultaneous Raw RF Capture


<picture>
<img src="assets/hardware-images/MISRC_V1.5_Tang_Nano_20k_Sony_ILCE-7RM3_2024.10.21_03.14.08-Small.png" width="600" height="" />
</picture>

> V1.5 with Tang Nano 20k on the FX3 to Tang adaptor PCB. 


## Description


MISRC is a device to capture two signals at 12-bit and up to 40 MHz (could maybe be extended to 80 MHz in the future) and an additional 8-bit's of binary (auxiliary data) over USB 3.0.

It is intended to capture modulated tape deck RF for software demodulation, but also baseband CVBS/S-Video (Composite) video signals for software decoding, and can be used as a direct stream Oscilloscope but limited to 2vpp input voltage.

The Decode Projects:

- [VHS-Decode](https://github.com/oyvindln/vhs-decode/)
- [HiFi-Decode](https://github.com/oyvindln/vhs-decode/wiki/hifi-decode)
- [CVBS-Decode](https://github.com/oyvindln/vhs-decode/wiki/CVBS-Composite-Decode) 

Provide decoding for a wide range of videotape formats, HiFi audio and even RAW or Baseband composite decoding with free and powerful software time base correction with full post filtering control over the signal processing.

-----------

Possible capture examples:

- Capture 2x CVBS
- Capture 1x S-Video (Y & C)
- Capture Video RF and HiFi RF simultaneously
- Capture Video RF and CVBS simultaneously
- Capture 4ch of 24-bit 48khz audio with AUX pins via integrated or external ADCs

> [!NOTE]
> It may be useful for other purposes as well, as it is built as a generic ADC with configurable filtering.


## Features


- Two 12-bit 40msps ADCs 
- Selectable input gain (8 steps)
- Selectable ADC range (1V or 2V)
- Selectable input impedance (75, 50, 37.5 and 30 ohms)
- DC or AC (pre- or post-termination) coupling
- Zero-adjust to compensate DC offset
- Latching clipping indicator
- Clock source selectable: USB PLL, crystal or external
  Clock output SMA for external devices
- Melted PCB Traces


## Costs


> [!TIP]  
> You can support the development and production of the MISRC platform [here](https://github.com/Stefan-Olt/MISRC/wiki/Donations).

- PCB: 20-30USD

- Parts 100-150USD

- Single unit total production cost is currently 260-300USD.

- [Order a V1.5 Development MISRC](https://github.com/Stefan-Olt/MISRC/wiki/Fabrication)


## Hardware 


| Media RF Type | MISRC Support |
| ------------- | ------------- |
| Video FM RF   | Yes           |
| HiFi FM RF    | Yes           |
| CVBS RF       | Yes           |
| S-Video RF    | Yes           |

- External Clock Source Output
- 6 Extra Aux inputs for audio ADC modules etc
- Duel ADC / Duel Input (BNC Connectors)
- Physically adjustable input filters
- 12-Bit, 20/40/65msps sampling
- [AD8138](https://www.analog.com/media/en/technical-documentation/data-sheets/ad8138.pdf) Driver, Op-Amp / [SN74ls541](https://www.ti.com/lit/ds/symlink/sn74ls540.pdf) Buffer / [AD9235](https://www.analog.com/media/en/technical-documentation/data-sheets/AD9235.pdf) ADC

------

> [!NOTE]  
> These are off-shelf PCBs and USB 3.0 devices that you add to the MISRC PCB.

- [Tang Nano 20k](https://s.click.aliexpress.com/e/_DcwBOX3) - buffer / data output over HDMI.
- [MS2130](http://en.macrosilicon.com/info.asp?base_id=2&third_id=75) - [Order Link 1](https://s.click.aliexpress.com/e/_DBaBiOp) / [Order Link 2](https://s.click.aliexpress.com/e/_okDl2Vf) - HDMI raw data stream capture.

> [!TIP]  
> You can order pre-made [adaptor PCBs here](https://ko-fi.com/s/617b72ab2c) for V1.5 boards with the headders for the FX3.  


# Software & Firmware 


> [!TIP]  
> Pre-built Binaries is available on the [releases tab](https://github.com/Stefan-Olt/MISRC/releases).

| Operating System  | MISRC Support          |
| ----------------- | ---------------------- |
| Microsoft Windows | Yes (10/11)            |
| Apple MacOS       | Yes                    |
| Apple MacOS ARM   | Yes                    |
| Linux             | Yes                    |
| Linux Arm         | Yes                    |


There are 2 tools currently and a few dependencies required to deploy a MISRC.

- MISRC Capture
- MISRC Extract
- hsdaoh library

> [!WARNING]  
> The main hsdaoh branch (steve-m) does not have the required changes merged yet, ensure the above liked repo is used for install or your application will not build.


<details closed>

<summary>hsdaoh Install Linux</summary>
<br>

To install the build dependencies on a distribution based on Debian (e.g. Ubuntu), run the following command:

    sudo apt-get install build-essential cmake pkgconf libusb-1.0-0-dev libuvc-dev

To build hsdaoh:

    git clone https://github.com/Stefan-Olt/hsdaoh.git
    mkdir hsdaoh/build
    cd hsdaoh/build
    cmake ../ -DINSTALL_UDEV_RULES=ON
    make -j 4
    sudo make install
    sudo ldconfig

To be able to access the USB device as non-root, the udev rules need to be installed (either use -DINSTALL_UDEV_RULES=ON or manually copy hsdaoh rules to /etc/udev/rules.d/).

Before being able to use the device as a non-root user, the udev rules need to be reloaded:

    sudo udevadm control -R
    sudo udevadm trigger

Furthermore, make sure your user is a member of the group 'plugdev'.
To make sure the group exists and add your user to it, run:

    sudo groupadd plugdev
    sudo usermod -a -G plugdev <your username>

If you haven't already been a member, you need to logout and login again for the group membership to become effective.


</details>


<details closed>

<summary>hsdaoh Install Windows</summary>
<br>

**Currently, this is highly experimental, noted here for testing and is not production implimented yet!**

Firstly download [Zadig](https://zadig.akeo.ie/)

Force the installation of `WinUSB (v6.1.7600.16385)` or `libusb-win32 (v1.2.6.0)` driver on your MS2130/MS2131 adapter, on `interface 0` leave `interface 4` alone. 

```
Interface 0 - USB Video
Interface 4 - HIDDevice
```

Install MSYS2 (https://www.msys2.org/)

Start `MSYS2 MINGW64` terminal from the application menu. (Blue Icon) 

Update all packages:

    pacman -Suy

Install the required dependencies:

    pacman -S git zip mingw-w64-x86_64-libusb mingw-w64-x86_64-libwinpthread mingw-w64-x86_64-cc \ mingw-w64-x86_64-gcc-libs mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja

Clone & Build libuvc:

    git clone https://github.com/steve-m/libuvc.git
    mkdir libuvc/build && cd libuvc/build
    cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=/mingw64
    cmake --build .
    cmake --install .

Build libhsdaoh

    cd ~
    git clone https://github.com/steve-m/hsdaoh.git
    mkdir hsdaoh/build && cd hsdaoh/build
    cmake ../
    cmake --build .

Gather all files required for release:

    zip -j hsdaoh_win_release.zip src/*.exe src/*.dll /mingw64/bin/libusb-1.0.dll /mingw64/bin/libuvc.dll /mingw64/bin/libwinpthread-1.dll


</details>

<details closed>

<summary>Software Install</summary>
<br>


Tested and built on Linux Mint 21.03 and 22 / Ubuntu 22.04

First install dependencies 

- `apt install libflac-dev`
- `hsdaoh driver`

These allow you to use the MS2130 & MS2131 chips directly.

Restart and then continue

Install capture & extract tools (Linux & MacOS)

    git clone https://github.com/Stefan-Olt/MISRC.git

Enter Directory

    cd MISRC/misrc_tools

Build and install 

```
mkdir build
cd build
cmake ..
make
sudo make install
```

Run `mirsc_extract` or `misrc_capture` in any directory without arguments to trigger the help menu.

There is a dedicated [sub-readme](/misrc_tools/README.md) for these tools.


</details>


</details>

<details closed>
<summary>Firmware Flashing </summary>
<br>


Today we are now using the more affordable [Tang Nano 20k](https://s.click.aliexpress.com/e/_DcwBOX3) sending the data over HDMI this only needs to be flashed once via usb connection, we have pre-compiled firmware see [releases](https://github.com/Stefan-Olt/MISRC/releases) for the latest version.

Install [openFPGALoader](https://github.com/trabucayre/openFPGALoader)

Connect your Tang to a USB 3.0 port via its Type-C, it will need this for 5V power after flashing, but not data.

Run via terminal inside the firmware directory

    openFPGALoader -b tangnano20k -f hsdaoh_nano20k_misrc.fs

You have flashed your Tang Nano 20k! 

</details>


## Capture


> [!TIP]  
> Pre-built Binaries is available on the [releases tab](https://github.com/Stefan-Olt/MISRC/releases).

`misrc_capture` is a simple command line interface program to capture from MISRC boards using [hsdaoh](https://github.com/Stefan-Olt/hsdaoh) to which leverages data capture over HDMI with [MS2130](https://s.click.aliexpress.com/e/_DBaBiOp) "U3" cheep HDMI capture cards that have YUV support and full-frame signal acesses. 

Create a folder which you wish to capture inside, open it inside terminal and then run `misrc_capture`.

Example with FLAC compression:
    
    misrc_capture -p -f -l 8 -a video_rf.flac -b hifi_rf.flac 

Example RAW:

    misrc_capture -a video_rf.s16 -b hifi_rf.s16


Example with AUX pins capture ([PCM1802 audio example](https://github.com/Stefan-Olt/MISRC/wiki/PCM-Extract))

    misrc_capture -p -f -l 8 -a video_rf.flac -b hifi_rf.flac -x pcm1802.bin

You can also define its directory path of each RF stream manually: 

    misrc_capture -p -f -l 8 -a /mnt/my_video_storrage/video_rf.flac -b ../../this/is/a/relative/path/hifi_rf.flac

Press <kbd>Ctrl</kbd>+<kbd>C</kbd> to copy and <kbd>Ctrl</kbd>+<kbd>P</kbd> to past your config from a notepad or txt file.

Use <kbd><</kbd>+<kbd>></kbd> to move edit position on the command line to edit the name or command while in terminal and <kbd>Enter</kbd> to run the command.

<kbd>Ctrl</kbd>+<kbd>C</kbd> Will kill the current process, use this to stop the capture manually.


<details closed>

<summary>Usage Arguments:</summary>
<br>


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

</details>


## Setting Up the MISRC


- Connect your 5v USB-C to the Tang Nano for power. 

- Connect your HDMI cable (copper or fibre) to your Tang Nano for data output to the MS2130 or MS2131. 

- Connect the desired sources to the BNC inputs and select suitable impedance and AC or DC coupling.

> [!NOTE]
> You will want to use 2vpp range rather than 1vpp range to make use of the 12-bits range.

Install [OCENAudio](https://www.ocenaudio.com/) or alternatives like [Audacity](https://www.audacityteam.org/download/) it will see captures as a `40khz 16-bit` file.

Run a 3-second test capture in FLAC, to be automatically reloaded for viewing. 

    misrc_capture -p -f -B -t 3 -a test.flac

Start the capturing process for setting the DC offset then gain:
    
    - Reset clipping LEDs (always on after start or major adjustment) 
    - Increase gain during capture until clipping LED lights up
    - Decrease gain one step and reset clipping LED
    - Repeat for the second channel if in use.

- Stop capture and verify levels are acceptable in your audio DAW and the signal is centred.

Once happy, then do a full test capture to verify. 

> [!CAUTION]
> - NEVER use USB for any other heavy-load task (like external HDD/SSD drives, USB network adaptors, YUV capture devices) during capture. 
> - Do not connect/disconnect any other USB device during capture, a dedicated USB 3.0 to 3.2 Gen 2 card is ideal for dedicated capture stations as it ensures dedicated bandwidth/power if you have other items that require USB.


## Design


MISRC is loosely based on the [Domesday Duplicator (DdD)](https://github.com/simoninns/DomesdayDuplicator) a LaserDisc focused ([ld-decode](https://github.com/happycube/ld-decode)) FM RF Archival device. 


It is built around the AD9235 analogue to digital converter by Analog Devices and is heavily based on the evaluation board circuit given in its datasheet with the AD8138 Op-Amp providing adjustable fixed gain.

The MISRC like the DdD it originally used the [Cypress FX3 SuperSpeed Explorer board](https://www.infineon.com/cms/en/product/evaluation-boards/cyusb3kit-003/) for a USB 3.0 data connection, and using Sigronk for capture, with hopes to not use the DE0 FPGA, this ended with the adoption of the Tang Nano 20k and MS2130 "data over YUV" method being used. 


## License

The hardware, firmware and software is released under different open-source licenses.
You can read the [License here](https://github.com/Stefan-Olt/MISRC/wiki/Licenses)
