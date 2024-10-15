# MISRC tools

## misrc_extract

Tool to extract the two ADC channels and the AUX data from the raw capture.


### Description

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
sudo make install
```

### Windows 

TBA - Binarys. 


# Usage

- Run tools without arguments to see options


## Capture 

A simple program to capture from MISRC using hsdaoh

Usage:

`-d` device_index (default: 0)  
`-n` number of samples to read (default: 0, infinite)  
`-a` ADC A output file (use '-' to write on stdout)  
`-b` ADC B output file (use '-' to write on stdout)  
`-x` AUX output file (use '-' to write on stdout)  
`-r` raw data output file (use '-' to write on stdout)  
`-p` pad lower 4 bits of 16 bit output with 0 instead of upper 4  
`-f` compress ADC output as FLAC  
`-l` LEVEL set flac compression level (default: 1)  
`-v` enable verification of flac encoder output  


## Extract

A simple program for extracting captured data into separate files

Usage:

`-i` Input file (use '-' to read from stdin)  
`-a` ADC A output file (use '-' to write on stdout)  
`-b` ADC B output file (use '-' to write on stdout)  
`-x` AUX output file (use '-' to write on stdout)  
`-p` pad lower 4 bits of 16 bit output with 0 instead of upper 4  
`-s` input is captured as single channel (-b cannot be used)  


## Version History

* 0.3.1
    * Fix: incorrect align could lead to crash (fix by namazso)

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
