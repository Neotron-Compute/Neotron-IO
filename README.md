# Neotron-IO

Firmware for the Neotron I/O Controller (a Microchip AtMega328), as fitted to the Neotron 32.

## Features

* PS/2 Keyboard Input
* PS/2 Mouse Input
* 2x Atari/SEGA MegaDrive/SEGA Genesis compatible 9-pin Joystick inputs
* Simple ASCII protocol to the Host CPU over UART

## Calibrating

The Neotron-32 hardware doesn't have a Crystal for the Neotron-IO chip. You must therefore configure it to use the 8 MHz internal-RC. Unfortunately the internal-RC is only accurate to ±10%, while for a functioning UART you need the clock to be withing ± 5%. To work around this, if you boot the device with pin PB0 held low, it enter Calibration Mode.

In Calibration Mode, the chip emits a 244.12 Hz square wave on pin PB0 (8 MHz divided by 32768). If you hold the Up button on your joystick then tap A, the OSCCAL calibration value is increased. If you hold Down and tap A, the OSCCAL calibration value is reduced. If you tap START, the OSCCAL value is saved to EEPROM. Use an oscilloscope and tune OSCCAL up and down until it is within 232 Hz to 256 Hz (and as close to 244.12 Hz as possible).

## Compiler

This project is built using [Arduino] and the [miniCore] add-on package. It has been tested using Arduino version 1.8.12 and miniCore version 2.0.4.

## Bootloader

Use the [miniCore] bootloader for the AtMega328P, configured to use the 8 MHz internal RC. You have to load the bootloader with an AVR programmer (such as an Arduino running the Arduino-ISP sketch, or a JTAG-ICE MkII). Once the bootloader is installer, you should be able to load a sketch over the UART immediately after reset (assuming your internal RC is within tolerance). If you have trouble, use an AVR programmer.

## Licence

The Arduino libraries are licensed under the GPL, and so this project is also licensed under the GPL (v3 or later version, at your choice).

See the [LICENCE](./LICENCE) file.

[Arduino]: https://www.arduino.cc
[miniCore]: https://github.com/MCUdude/MiniCore
