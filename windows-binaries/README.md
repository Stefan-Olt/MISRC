# Windows Pre-built Binaries

This directory contains pre-built Windows binaries for the MISRC project.

## Contents

- `misrc_capture.exe` - Main capture application
- `libhsdaoh.dll` - HSDAOH library for MS2130/MS2131 HDMI capture device support
- `libuvc.dll` - USB Video Class library
- `libusb-1.0.dll` - USB library for device communication
- `libFLAC.dll` - FLAC compression library
- `libogg-0.dll` - Ogg container format library  
- `libwinpthread-1.dll` - Windows pthread implementation
- `zadig-2.9.exe` - USB driver installation tool (for MS2130/MS2131 devices)

## Usage

These binaries are compiled for Windows x64 using MSYS2/MinGW64 and include all necessary dependencies.

To use:
1. Download the complete Windows release package from GitHub releases
2. Extract all files to a directory
3. Install the WinUSB driver for your MS2130/MS2131 device using Zadig
4. Run `misrc_capture.exe` from the command line

## Driver Installation

Before using MISRC on Windows, you need to install the proper USB driver:

1. Run `zadig-2.9.exe` as administrator
2. Select your MS2130/MS2131 device
3. Install `WinUSB (v6.1.7600.16385)` or `libusb-win32 (v1.2.6.0)` driver on Interface 0
4. Leave Interface 4 (HID Device) unchanged

## Build Information

These binaries were built using the MSYS2 environment following the instructions in the main README.md file.
