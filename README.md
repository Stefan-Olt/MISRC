# MISRC - Multi Input Simultaneous Raw RF Capture

[Hardware](#hardware-features) - [Firmware](#firmware-tang-nano-20k) - [Software](#software) - [Usage example](#capture--usage-example)

<picture>
<img src="assets/hardware-images/MISRC_V1.5_Tang_Nano_20k_Sony_ILCE-7RM3_2024.10.21_03.14.08-Small.png" width="600" height="" />
</picture>

> V1.5 with Tang Nano 20k on the FX3 to Tang Nano 20k adapter PCB. 

## Description


MISRC is a device to capture two signals at 12-bit and up to 40 MSPS (20 Mhz bandwith) (This could be extended to 80 MSPS in the future) and an additional 8-bit's (2-bit's for v2.5) of binary (auxiliary data) over USB 3.0.

It is intended to capture modulated tape deck RF for software demodulation, but also baseband CVBS/S-Video (Composite) video signals for software decoding, and can be used as a direct stream Oscilloscope but limited to 2vpp input voltage.

The Decode Projects:

- [VHS-Decode](https://github.com/oyvindln/vhs-decode/) / [Tape Decode](https://github.com/harrypm/tape-decode-rust)
- [HiFi-Decode](https://github.com/oyvindln/vhs-decode/wiki/hifi-decode)
- [CVBS-Decode](https://github.com/oyvindln/vhs-decode/wiki/CVBS-Composite-Decode) 

Provide decoding for a wide range of videotape formats, HiFi audio and even RAW or Baseband composite decoding with free and powerful software time base correction with full post filtering control over the signal processing.

-----------

| Media RF Type | MISRC Support |
| ------------- | ------------- |
| Video FM RF   | Yes           |
| HiFi FM RF    | Yes           |
| CVBS RF       | Yes           |
| S-Video RF    | Yes           |

Possible capture examples:

- Capture 2x CVBS
- Capture 1x S-Video (Y & C)
- Capture Video RF and HiFi RF simultaneously
- Capture Video RF and CVBS simultaneously
- Capture 4ch of 24-bit 48khz audio with AUX pins via integrated or external clock-locked ADCs See [MISRC Clockgen]()

> [!NOTE]
> It may be useful for other purposes as well, as it is built as a generic ADC with configurable filtering.


## Hardware features


- Dual ADC / Dual Input (BNC Connectors)
- 6 Extra Aux inputs for audio ADC modules etc
- Two 12-bit 40msps ADCs: [AD9235](https://www.analog.com/media/en/technical-documentation/data-sheets/AD9235.pdf)
- Selectable input gain (8 steps) (using [AD8138](https://www.analog.com/media/en/technical-documentation/data-sheets/ad8138.pdf) op-amp)
- Selectable ADC range (1V or 2V)
- Selectable input impedance (75, 50, 37.5 and 30 ohms)
- DC or AC (pre- or post-termination) coupling
- Zero-adjust to compensate DC offset
- Latching clipping indicator (automatic resetting on v2.5) 
- Clock source selectable: USB PLL (FX3 setup), crystal or external
- Clock output SMA for external devices (In/Out for V2.5)
- Melted PCB Traces


## Costs

> [!TIP]  
> You can support the development and production of the MISRC platform [here](https://github.com/Stefan-Olt/MISRC/wiki/Donations).
- PCB: 20-30USD
- Parts 100-150USD
- Single unit total production cost is currently 260-350USD. (USA costs vastly higher for direct fab)
- [Order a V1.5 Development MISRC](https://github.com/Stefan-Olt/MISRC/wiki/Fabrication)

------

> [!NOTE]  
> These are off-shelf PCBs and USB 3.0 devices that you add to the MISRC PCB.

- [Tang Nano 20k](https://s.click.aliexpress.com/e/_DcwBOX3) - buffer / data output over HDMI.
- [MS2130](http://en.macrosilicon.com/info.asp?base_id=2&third_id=75) - [Order Link 1](https://s.click.aliexpress.com/e/_DBaBiOp) / [Order Link 2](https://s.click.aliexpress.com/e/_okDl2Vf) - HDMI raw data stream capture.

> [!TIP]  
> You can order pre-made [adaptor PCBs here](https://ko-fi.com/s/617b72ab2c) for V1.5 boards with the headers for the FX3.  


## Firmware (Tang Nano 20k)


To be able to use the [Tang Nano 20k](https://s.click.aliexpress.com/e/_DcwBOX3) to send data over HDMI, it needs to be flashed once via USB connection:

<details closed>
<summary>Firmware Flashing </summary>
<br>

1. Download the firmware, see [releases](https://github.com/Stefan-Olt/MISRC/releases) for the latest version.

2. Install [openFPGALoader](https://github.com/trabucayre/openFPGALoader)

3. Connect your Tang to a USB 3.0 port via its Type-C, it will need this for 5V power after flashing, but not data from the MISRC.

   Run via terminal inside the firmware directory

       openFPGALoader -b tangnano20k -f hsdaoh_nano20k_misrc.fs

You have flashed your Tang Nano 20k! 

</details>


## Software


[MISRC GUI](https://github.com/harrypm/MISRC-GUI#misrc-gui) is built for cross platform use, all binary's bundle both CLI and GUI. 

<img width="" height="" alt="" src="https://github.com/harrypm/MISRC-GUI/blob/main/assets/images/MISRC_GUI_Window_Main_Current.png" />


For detailed information of the original core tools, see the [misrc_tools readme](/misrc_tools/README.md).

<details closed>
<summary>Windows Driver Setup</summary>
<br>
   
For `misrc_capture` or `misrc-gui` to be able to access the MS2130 or MS2131 capture device, you need to install a generic driver:

Firstly download [Zadig](https://zadig.akeo.ie/)

Force the installation of `WinUSB (v6.1.7600.16385)` or `libusb-win32 (v1.2.6.0)` driver on your MS2130/MS2131 adapter, on `interface 0` leave `interface 4` alone. 

```
Interface 0 - USB Video
Interface 4 - HIDDevice
```

</details>

<details closed>
<summary>Install macOS</summary>
<br>

For macOS you do not need to install anything.

- macOS versions 10.15 and 11: You can directly execute `misrc_capture`.
- macOS 12 and later: You have to run `misrc_capture` as root (sudo). `misrc-gui` automatically prompts when opened. This is a new security feature by Apple, there is currently no way to bypass it...

</details>

<details closed>
<summary>Install Linux</summary>
<br>

If you want to run `misrc_capture` as root, there is no need to install anything on Linux.

But for `misrc_capture` to be able to access the MS2130 capture device without root privilege (sudo), you may need to install a udev-rule and add the user that will use `misrc_capture` to the `plugdev`-group.
The pre-build packages contains the script `install-udev-rules.sh`. On execution (double-click the file or run `./install-udev-rules.sh` on the command line) it will install the rule and add the current user to the plugdev group.

</details>


## Setting Up the MISRC


- Connect your 5V USB-C to the Tang Nano for power. 

- Connect your HDMI cable (copper or fiber) to your Tang Nano for data output to the MS2130 or MS2131. 

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


MISRC is loosely based on the [Domesday Duplicator (DdD)](https://github.com/harrypm/DomesdayDuplicator) a legacy LaserDisc capture focused FM RF Archival device. 

The MISRC is built around the AD9235 analogue to digital converter by Analog Devices and is heavily based on the evaluation board circuit given in its datasheet with the AD8138 Op-Amp providing adjustable fixed gain.

The MISRC like the DdD it originally used the [Cypress FX3 SuperSpeed Explorer board](https://www.infineon.com/cms/en/product/evaluation-boards/cyusb3kit-003/) for a USB 3.0 data connection, and using Sigronk for capture, with hopes to not use the DE0 FPGA, this ended with the adoption of the Tang Nano 20k and MS2130 "data over YUV" method being used. 


## License


The hardware, firmware and software is released under different open-source licenses.
You can read the [License here](https://github.com/Stefan-Olt/MISRC/wiki/Licenses)
