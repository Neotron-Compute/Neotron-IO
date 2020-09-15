# Neotron-IO

Firmware for the Neotron I/O Controller (a Microchip AtMega328), as fitted to the Neotron 32.

## Features

* PS/2 Keyboard Input
* PS/2 Mouse Input
* 2x Atari/SEGA MegaDrive/SEGA Genesis compatible 9-pin Joystick inputs
* Simple ASCII protocol to the Host CPU over UART

## Pinout

### Neotron 32 v1.2.1

| AtMega Pin | Arduino Name | Direction  | Function              |
|:-----------|:-------------|:-----------|:----------------------|
| PB0        | D8           | N/C        | Not used              |
| PB1        | D9           | N/C        | Not used              |
| PB2        | D10          | In         | `START_C_JS2`         |
| PB3        | D11          | In         | `RIGHT_GND_JS2 + SCK` |
| PB4        | D12          | In         | `LEFT_GND_JS2 + MISO` |
| PB5        | D13          | In         | `DOWN_JS2 + MOSI`     |
| PB6        | XTAL1        | In         | `A_B_JS2`             |
| PB7        | XTAL2        | In         | `UP_JS2`              |
| PC0        | A0           | Open-Drain | `KB_CLK`              |
| PC1        | A1           | Open-Drain | `KB_DAT`              |
| PC2        | A2           | Open-Drain | `MS_CLK`              |
| PC3        | A3           | Open-Drain | `MS_DAT`              |
| PC4        | A4           | Out        | `SELECT_JS1`          |
| PC5        | A5           | Out        | `SELECT_JS2`          |
| PC6        | RESET        | In         | `/RESET`              |
| PD0        | D0           | Out        | `KBMS_TO_HOST`        |
| PD1        | D1           | In         | `KBMS_FROM_HOST`      |
| PD2        | D2           | In         | `START_C_JS1`         |
| PD3        | D3           | In         | `RIGHT_GND_JS1`       |
| PD4        | D4           | In         | `LEFT_GND_JS1`        |
| PD5        | D5           | In         | `DOWN_JS1`            |
| PD6        | D6           | In         | `A_B_JS1`             |
| PD7        | D7           | In         | `UP_JS1`              |

## UART Interface

This version of Netron IO implements a basic set of commands over the UART. Each command is plain ASCII, and is terminated by a new-line character (`\n`). Carriage-return characters (`\r`) are ignored, and any raw data is sent hex-encoded.

### Indications

#### Booted

```
B020
```

The `B` indicates that the device has booted. The following three hex digits are the version number, as major, minor and patch.

#### Joystick 1 Changed

```
Sxxxx
```

Indicates Joystick 1 has changed state. The 16-bit hex word `xxxx` indicates the state of the (up to) 16 supported pins on the Joystick interface.

The currently supported bits are (where 0 is the least significant bit):

* Up: Bit 0, (or 0x0001)
* Down: Bit 1 (or 0x0002)
* Left: Bit 2 (or 0x0004)
* Right: Bit 3 (or 0x0008)
* A: Bit 4 (or 0x0010)
* B: Bit 5 (or 0x0020)
* C: Bit 6 (or 0x0040)
* Start: Bit 7 (or 0x0080)

In the future we may support:

* X: Bit 8 (or 0x0100)
* Y: Bit 9 (or 0x0200)
* Z: Bit 10 (or 0x0400)

For example, a value of `0x0042` means:

* Down is pressed (`0x0002`)
* C is pressed (`0x0040`)
* No other buttons are pressed

#### Joystick 2 Changed

```
Txxxx
```

Indicates Joystick 2 has changed state. The 16-bit hex word `xxxx` indicates the state of the (up to) 16 supported pins on the Joystick interface. See the *Joystick 1 Changed* indication for more details.

#### Keyboard Byte Available

```
Kxx
```

Indicates that the Keyboard has delivered a byte over the keyboard PS/2 interface. The hex byte `xx` is the byte that was received.

#### Mouse Byte Available


```
Mxx
```

Indicates that the Mouse has delivered a byte over the mouse PS/2 interface. The hex byte `xx` is the byte that was received.

### Commands

#### Send byte to Keyboard

```
Kxx
```

The `K` command sends the following hex byte (`xx`) to the keyboard PS/2 port. The Neotron IO controller will automatically hold the PS/2 device's clock line to signify that there is data to be sent, and clock out each byte in turn.

#### Send byte to Mouse

```
Mxx
```

The `M` command sends the following hex bytes (`xx`) to the mouse PS/2 port. The Neotron IO controller will automatically hold the PS/2 device's clock line to signify that there is data to be sent, and clock out each byte in turn.

## Calibrating

The Neotron-32 hardware doesn't have a Crystal for the Neotron-IO chip. You must therefore configure it to use the 8 MHz internal-RC. Unfortunately the internal-RC is only accurate to ±10%, while for a functioning UART you need the clock to be within ± 5%. To work around this, if you boot the device with pin PB0 held low, it enter its custom Calibration Mode.

In Calibration Mode, the Neotron-IO chip emits a 244.12 Hz square wave on pin PB0 (8 MHz divided by 32768). If you hold the Up button on your joystick then tap A, the OSCCAL calibration value is increased. If you hold Down and tap A, the OSCCAL calibration value is reduced. If you tap START, the OSCCAL value is saved to EEPROM. Use an oscilloscope and tune OSCCAL up and down until it is within 232 Hz to 256 Hz (and as close to 244.12 Hz as possible).

## Compiler

This project is built using [Arduino] and the [miniCore] add-on package. It has been tested using Arduino version 1.8.12 and miniCore version 2.0.4.

## Bootloader

Use the [miniCore] bootloader for the AtMega328P, configured to use the 8 MHz internal-RC. You have to load the bootloader with an AVR programmer (such as an Arduino running the [ArduinoISP sketch], or a dedicated [Atmel-ICE] USB programmer). Once the bootloader is installed, you should be able to load a sketch over the UART immediately after reset (assuming your internal-RC is within tolerance). If you have trouble, use an AVR programmer.

## Licence

The Arduino libraries are licensed under the GPL, and so this project is also licensed under the GPL (v3 or later version, at your choice).

See the [LICENCE](./LICENCE) file.

[Arduino]: https://www.arduino.cc
[miniCore]: https://github.com/MCUdude/MiniCore
[ArduinoISP sketch]: https://www.arduino.cc/en/Tutorial/ArduinoISP
[Atmel-ICE]: https://www.microchip.com/Developmenttools/ProductDetails/ATATMEL-ICE
