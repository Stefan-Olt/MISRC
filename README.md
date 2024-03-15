# MISRC - Multi Input Simultaneous Raw RF Capture

<picture>
  <source srcset="assets/hardware-images/MISRC-v0.0.5-Transparent-Hand-Soldered.jxl" type="image/jxl" width="600" />
  <source srcset="assets/hardware-images/MISRC-v0.0.5-Transparent-Hand-Soldered.avif" type="image/avif" width="600" />
  <img src="assets/hardware-images/MISRC-v0.0.5-Transparent-Hand-Soldered.png" width="600" height="" />
</picture>


## Description


MISRC is a device to capture two signals at 12-bit and up to 40 MHz (could maybe extended to 80 MHz in future) and additional 6 bits binary (auxillary data) over USB3.
It is intended to capture modulated tape deck RF for software demodulation, but also CVBS (Composite) video signals for software decoding.

The [VHS-Decode](https://github.com/oyvindln/vhs-decode/), [HiFi-Decode](https://github.com/oyvindln/vhs-decode/wiki/hifi-decode), [CVBS-Decode](https://github.com/oyvindln/vhs-decode/wiki/CVBS-Composite-Decode) projects provides software decoding for many video tape formats, hifi-audio via hifi-decode and RAW CVBS decoding via cvbs-decode

Possible capture examples:

- Capture 2x CVBS
- Capture 1x S-Video (Y & C)
- Capture Video RF and HiFi RF simultaneously
- Capture Video RF and CVBS simultaneously

It may be useful for other purposes as well, as it is built as a generic ADC with configerable filtering.


## Features


- Two 12-bit ADCs
- Selectable input gain (8 steps)
- Selectable ADC range (1V or 2V)
- Selectable input impedance (75, 50, 37.5 and 30 ohms)
- DC or AC (pre- or post-termination) coupling
- Zero-adjust to compensate DC offset
- Latching clipping indicator
- Clock source selectable: USB PLL, crystal or external
- Output clock to external device
- Melted PCB Traces

## Costs


PCB: 20-30USD
Parts 100-120USD

(Exact prices and Gerbers/BOM comming soon) 


## Capture process



Capturing can be done using the [Sigrok-cli](https://sigrok.org/wiki/Downloads#Binaries_and_distribution_packages) application using the following command:


    sigrok-cli --driver cypress-fx3 --output-format binary --config samplerate=40m --continuous --output-file MISRC_Capture.bin


Note that `sigrok-cli` will most likely terminate on the first call with the message that no device was found (it seems it does not rescan the USB bus after the firmware is being uploaded), the second time it should work fine.

The output `MISRC_Capture.bin` file will contain all data interleaved, you have to use the [extraction tool](https://github.com/Stefan-Olt/MISRC/tree/main/misrc_extract) to extract one or both channels and the aux data to seprate files.


## User Guide

Step-by-step 

* Connect the device with a high-quality USB 3.0 Type-B to A or C cable to your PC. 

NOTE: NEVER use USB for any other heavy-load task (like external HDD/SSD drives, USB network adapters, YUV capture devices) during capture. Do not connect/disconnect any other USB device during capture.

* Connect the desired sources to the BNC inputs and select suitable impedance and coupling

* Start the capturing process for setting the gain:
    * Reset clipping LEDs (always on after start)
    * Increase gain during capture until clipping LED lights up
    * Decrease gain one step and reset clipping LED
    * Repeat for second channel if in use

* Stop capture and verify levels are acceptable in a audio editor (for example Audacity)

* Start capture.


## Design


MISRC is loosly based on the [Domesday Duplicator (DdD)](https://github.com/simoninns/DomesdayDuplicator). Like the DdD it uses the Cypress FX3 SuperSpeed Explorer board, but does not require a FPGA board.
It is build around the AD9235 analog-digital converter by Analog Devices and is heavily based on the evaluation board circuit given in its datasheet.  


## Hardware 


- Offshelf FX3 USB 3.0 board (same as DdD)
- External Clock Source Output
- 6 Extra Aux inputs for audio ADC modules etc
- Duel ADC / Dual Input (BNC Connectors)
- Physically adjustable input filters
- 12-Bit, 20/40/65msps sampling
- AD8138 Driver, Op-Amp / SN74ls541 Buffer / AD9235 ADC


## Firmware


MISRC uses a (modified) firmware for the FX3 provided by Infineon (Cypress) in the [Infineon Developer Community](https://community.infineon.com/t5/USB-superspeed-peripherals/EZ-USB-FX3-Explorer-kit-as-16-channel-logic-analyzers-gt-dropped-samples-after/m-p/635325/) 
Modifications:
* Change bus width in GPIF II Designer to 32 bit instead of 16 bit
* Change the top value of the counter variable from 8191 to 4095 in the state machine diagram
* Select external clock source (only for building a firmware to use external clock/oscillator)


### Getting the firmware


It is unclear if the modified source and/or binary can be provided here, for now you have to register at Infineon, download and install the [EZ-USB FX3 SDK](https://www.infineon.com/cms/de/design-support/tools/sdk/usb-controllers-sdk/ez-usb-fx3-software-development-kit/), and download, modify and build the source to get the firmware.


## Sigrok


As MISRC uses a (modified) firmware provided by Infineon to work with [sigrok](https://sigrok.org/) (PulseView), the sigrok-cli interface available for many platforms is used to capture.

Unfortunately because of the unclear firmware license, the sigrok project has not (yet?) merged the [pull request to add Cypress FX3 support](https://github.com/sigrokproject/libsigrok/pull/148/).
You have to apply this patch and build sigrok for your platform.

Note: The other, completly free, open-source firmware projects mentioned in the pull request do not support USB3 correctly yet, making them unuseable for MISRC.


## License


You can read the [Licence here](https://github.com/Stefan-Olt/MISRC/wiki/Licence).
