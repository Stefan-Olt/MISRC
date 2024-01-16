# MISRC

Multi-input simultaneous raw/RF capture

## Description

MISRC is a device to capture two signals at 12-bit and up to 40 MHz (could maybe extended to 80 MHz in future) and additional 6 bits binary (auxillary data) over USB3.
It is intended to capture modulated tape deck RF for software demodulation, but also (C)VBS video signal for software decoding.
The [VHS-Decode](https://github.com/oyvindln/vhs-decode/) project provides software decoding for many video tape formats and CVBS decoding.

Possible capture examples:
* Capture video RF and hi-fi audio RF simultaneously
* Capture video RF and CVBS simultaneously
* Capture S-Video luminance / chrominance separated but simultaneously 

It may be useful for other purposes as well, as it is build as a generic ADC.

## Features
* Two 12-bit ADCs
* Selectable input gain (8 steps)
* Selectable ADC range (1V or 2V)
* Selectable input impedance (75, 50, 37.5 and 30 ohms)
* DC or AC (pre- or post-termination) coupling
* Zero-adjust to compensate DC offset
* Latching clipping indicator
* Clock source selectable: USB PLL, crystal or external
* Output clock to external device

## Pictures

<img src="hardware/images/misrc_v0.0.5.jpg" width="" height="">

## Capture process
Capturing can be done using the `sigrok-cli` application using the following command:
```
sigrok-cli --driver cypress-fx3 --output-format binary --config samplerate=40m --continuous --output-file CAPTURE.BIN
```
Note that `sigrok-cli` will most likely terminate on the first call with the message that no device was found (it seems it does not rescan the USB bus after the firmware is being uploaded), the second time it should work fine.

The output file will contain all data interleaved, use [misrc_extract](https://github.com/Stefan-Olt/MISRC/tree/main/misrc_extract) to extract one or both channels and the aux data.

### Step-by-step 
* Connect the device with a high-quality USB cable to your PC. Do not use USB for any other heavy-load task (like external drives, USB network adapters etc.) during capture. Do not connect/disconnect any other USB device during capture.
* Connect the desired sources to the BNC inputs and select suitable impedance and coupling
* Start the capturing process for setting the gain:
    * Reset clipping LEDs (always on after start)
    * Increase gain during capture until clipping LED lights up
    * Decrease gain one step and reset clipping LED
    * Repeat for second channel if in use
* Stop capture and verify levels are acceptable in a audio editor (for example Audacity)
* Start capture

## Design
MISRC is loosly based on the [Domesday Duplicator (DdD)](https://github.com/simoninns/DomesdayDuplicator). Like the DdD it uses the Cypress FX3 SuperSpeed Explorer board, but does not require a FPGA board.
It is build around the AD9235 analog-digital converter by Analog Devices and is heavily based on the evaluation board circuit given in its datasheet.  

## Firmware

MISRC uses a (modified) firmware provided by Infineon (Cypress) in the [Infineon Developer Community](https://community.infineon.com/t5/USB-superspeed-peripherals/EZ-USB-FX3-Explorer-kit-as-16-channel-logic-analyzers-gt-dropped-samples-after/m-p/635325/) 
Modifications:
* Change bus width in GPIF II Designer to 32 bit instead of 16 bit
* Change the top value of the counter variable from 8191 to 4095 in the state machine diagram
* Select external clock source (only for building a firmware to use external clock/oscillator)

### Getting the firmware

It is unclear if the modified source and/or binary can be provided here, for now you have to register at Infineon, download and install the [EZ-USB FX3 SDK](https://www.infineon.com/cms/de/design-support/tools/sdk/usb-controllers-sdk/ez-usb-fx3-software-development-kit/),
and download, modify and build the source to get the firmware.

## sigrok

As MISRC uses a (modified) firmware provided by Infineon to work with [sigrok](https://sigrok.org/) (PulseView), the sigrok-cli interface available for many platforms is used to capture.
Unfortunately because of the unclear firmware license, the sigrok project has not (yet?) merged the [pull request to add Cypress FX3 support](https://github.com/sigrokproject/libsigrok/pull/148/).
You have to apply this patch and build sigrok for your platform.

Note: The other, completly free, open-source firmware projects mentioned in the pull request do not support USB3 correctly yet, making them unuseable for MISRC.

## License

### Hardware: Creative Commons BY-SA 4.0

Please see the following link for details: https://creativecommons.org/licenses/by-sa/4.0/

You are free to:

Share - copy and redistribute the material in any medium or format
Adapt - remix, transform, and build upon the material
for any purpose, even commercially.

This license is acceptable for Free Cultural Works.

The licensor cannot revoke these freedoms as long as you follow the license terms.

Under the following terms:

Attribution - You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.

ShareAlike - If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

No additional restrictions - You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.

### Software: GNU GPL v3 (or later)

    MISRC is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MISRC is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
