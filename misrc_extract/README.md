# MISRC extract

Tool to extract the two ADC channels and the AUX data from the raw capture.

## Description

The 2 ADC channels with 12 bit each and the 8 bit AUX data is interleaved into 32 bit words.
This tool will deinterleave the data. For x86_64 (64 bit AMD and Intel processors)
there is handwritten assembly for higher performance using SSE instructions.

## Building

### Linux, macOS, BSD etc.

```
mkdir build
cd build
cmake ..
make
make install
```

### Windows

TBD

## Usage

* Run tool without arguments to see options

## Version History
* 0.3
    * New: allow extraction of data from single channel capture

* 0.2
    * Fix: signal now with correct polarity
    * Mod: output program name and version
    * New: option to pad lower 4 bits instead of upper 4

* 0.1
    * Initial Release

## License

This project is licensed under the GNU General Public License v3 or later - see the LICENSE file for details
