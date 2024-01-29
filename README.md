# Purpose

This project enables a Raspberry Pi Pico W to become a standalone RF-enabled
APRS IGate.

Just connect a BaoFeng radio to a Raspberry Pi Pico W and done! ;)

No slow-to-boot, and finicky Raspberry Pi(s) are required anymore!

## Usage

Customize `pico_tnc/config.h` with your values.

Build the code using Arduino IDE.

See `schematic.pdf` for the schematics (connections to make).

## Notes

You do NOT need any "special BaoFeng cable". We will make one ourselves ;)

We also recommend connecting a `ASAIR AHT25 Integrated temperature and humidity
sensor` to the Raspberry Pi Pico W for added fun.
