# History

## Revision 0.0.5

* First hardware build

## Revision 0.0.6

* Fix: Added capacitors at output from inverting charge pump to stabilize operation
* Mod: Changed zero adjust from center voltage control to inverting input offset to allow wider adjustment range (untested)
* Mod: Changed feedback resistor 1k3 to 1k to allow lower gains (useful for CVBS capture)
* Mod: Changed selectable impedances to 75, 50, 37.5 and 30 Ohms (useful for CVBS capture)
* Mod: Increased coupling capacitor values to 100µF and 470nF (useful for CVBS capture)
* Mod: Remove copper around screw holes to avoid possible ground connection when soldermask gets scratched
* New: Added selectable crystal oscillator as clock source
* New: Added input protection with transistor zener clamp (untested)


## V1.5


Official production release version 1.0 effectively 30-50 units in the wild.

- BNC changed too much cheaper and case practical DOSIN
- Adaptor PCB developed for using the tang nank 20k and hsdaoh workflow
- 3D printable case designs
- Updated Bill of Materials


## V1.5a 

- Silkscreen cleanups 
- SMA changed to a more generic format (supporting all cables)
- Crystal package updated to a larger format, with higher load for PCM1802 boards support
- Updated Bill of Materials

