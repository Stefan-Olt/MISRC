# History
## Revision 0.0.6
* Fix: Added capacitors at output from inverting charge pump to stabilize operation
* Mod: Changed zero adjust from center voltage control to inverting input offset to allow wider adjustment range (untested)
* Mod: Changed feedback resistor 1k3 to 1k to allow lower gains (useful for CVBS capture)
* Mod: Changed selectable impedances to 75, 50, 37.5 and 30 Ohms (useful for CVBS capture)
* Mod: Increased coupling capacitor values to 100µF and 470nF (useful for CVBS capture)
* Mod: Remove copper around screw holes to avoid possible ground connection when soldermask gets scratched
* New: Added selectable crystal oscillator as clock source
* New: Added input protection with transistor zener clamp (untested)

## Revision 0.0.5
* First hardware build
