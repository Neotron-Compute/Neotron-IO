# Neotron-IO

Firmware for the Neotron I/O Controller (a Microchip AtMega328), as fitted to the Neotron 32 and other Neotron systems.

## Features

* PS/2 Keyboard Input
* PS/2 Mouse Input
* I²C Interface, implementing the Microsoft [HID over I²C] Protocol
* Debug UART Interface
* Arduino Uno Compatible

## Pinout

| Pin | Arduino Name | Function            | Direction |
|-----|--------------|---------------------|-----------|
| PD0 | D0 / UART RX | UART Receive        | In        |
| PD1 | D1 / UART TX | UART Transmit       | Out       |
| PD2 | D2           | Keyboard PS/2 Clock | Bi-Dir    |
| PD3 | D3           | Keyboard PS/2 Data  | Bi-Dir    |
| PD4 | D4           | Mouse PS/2 Clock    | Bi-Dir    |
| PD5 | D5           | Mouse PS/2 Data     | Bi-Dir    |
| PD6 | D6           | System Reset        | Output    |
| PD7 | D7           | Power Switch        | Input     |
| PB0 | D8           | Unused              | Unused    |
| PB1 | D9           | Power LED           | Out       |
| PB2 | D10          | Fault LED           | Out       |
| PB3 | D11          | ISP (MOSI)          | In        |
| PB4 | D12          | ISP (MISO)          | Out       |
| PB5 | D13          | ISP (SCK)           | In        |
| PB6 | -- (Crystal) |                     | N/A       |
| PB7 | -- (Crystal) |                     | N/A       |
| PC0 | A0           | 5V Monitor          | In        |
| PC1 | A1           | 3.3V Monitor        | In        |
| PC2 | A2           | Power Enable        | Out       |
| PC3 | A3           | Interrupt Line      | Out       |
| PC4 | A4           | I²C Data (SDA)      | Bi-Dir    |
| PC5 | A5           | I²C Clock (SCL)     | In        |
| PC6 | ~RESET       | ISP (RESET)         | In        |

### Power LED

We light this when commanded to over the HID. It is exposed as an LED.

### Fault LED

We light this when commanded to over the HID. It is exposed as an LED.

### 5V Monitor

We divide down the 5.0V rail to below 1.1V and read it using the AtMega's ADC.

### 3.3V Monitor

We divide down the 3.3V rail to below 1.1V and read it using the AtMega's ADC.

### Interrupt Line

This goes low whenever there is data to read from the HID controller. It goes low once there is no longer any data to read (i.e. it's an edge triggered interrupt).

### I²C Bus

The AtMega is an I2C peripheral device with a 7-bit address of 0x6F. It implements the Microsoft HID (Human Interface Device) over I²C protocol, which is just the USB HID protocol adapted to work over I²C. It was originally developed for Windows 8 tablets that didn't have legacy i8042 keyboard controllers.

### Keyboard PS/2 Bus

Both lines should be pulled up with 4.7k to 5V. Both pins are open-drain - from the MCU's point of view they are either inputs, or driven hard low, but never driven hard high.

### Mouse PS/2 Bus

This is the same as the Keyboard PS/2 bus. The only difference is how we package the bytes recevied, and that we put the mouse into automatic mode on start-up.

### System Reset

The system reset line is held low for 500 ms after power up, and then released (the pin is set as an input, and it has an external pull-up). This allows all the power supplies to stablise before the chips are taken out of reset.

### Power Switch / Power Enable

When the system is off, the main power input FET is enabled when the power switch is pressed (and pulls both `~POWER_EN` and pin PD7 to ground). Once the AtMega has started, it drives `PC2` low to keep `~POWER_EN` low even once the power switch has been released. Provided the switch is pressed for longer than it takes the AtMega to boot, the system is then running.

If the power switch is pressed when the system is running, the AtMega detects this and reports the switch state over HID to the host processor. The host processor should then perform a clean-shutdown, and command the AtMega to clear the `~POWER_EN` line over HID as the last thing it does.

If the power switch continues to be held for 5 seconds, and the host hasn't indicated that it is ready to shutdown, the AtMega drops the `~POWER_EN` line, cutting power to the system.

**TODO: ARGH. The schematic won't work as drawn. If POWER_EN is driven hard low, and the POWER_SW pin is pulled weak high (say, 100k internal pull-ups), the power switch line floats somewhere between the two (it has 100k to Vcc and 10k to GND).**

## Calibrating

The Neotron-32 hardware doesn't have a Crystal for the Neotron-IO chip. You must therefore configure it to use the 8 MHz internal-RC. Unfortunately the internal-RC is only accurate to ±10%, while for a functioning UART you need the clock to be within ± 5%. To work around this, if you boot the device with pin PB0 held low, it enters its custom Calibration Mode.

In Calibration Mode, the Neotron-IO chip emits a 244.12 Hz square wave on pin PB0 (8 MHz divided by 32768). If you tap 'a' on the keyboard, the OSCCAL calibration value is increased. If you tap 'z', the OSCCAL calibration value is reduced. If you tap the space bar, the OSCCAL value is saved to EEPROM. Use an oscilloscope and tune OSCCAL up and down until it is within 232 Hz to 256 Hz (and as close to 244.12 Hz as possible).

## Protocol

This device implements the [HID over I²C] protocol, which is based on [HID over USB] but adapted to operate over an I²C bus. The Neotron IO chip is the I²C *DEVICE*, and the main CPU of the Neotron system is the *HOST*.

### Input Top-Level Collections

The Neotron-IO device exposes multiple top-level collections for Input:

1. Keyboard
2. Mouse
3. Joystick 1
4. Joystick 2
5. Buttons

### Output Top-Level Collections

The Neotron-IO device exposes multiple top-level collections for Output:

1. Keyboard LEDs
2. System LEDs

## Compiler

This project is built using [Arduino] and the [miniCore] add-on package. It has been tested using Arduino version 1.8.12 and miniCore version 2.0.4.

## Bootloader

Use the [miniCore] bootloader for the AtMega328P, configured to use the 8 MHz internal-RC. You have to load the bootloader with an AVR programmer (such as an Arduino running the [ArduinoISP sketch], or a dedicated [Atmel-ICE] USB programmer). Once the bootloader is installed, you should be able to load a sketch over the UART immediately after reset (assuming your internal-RC is within tolerance). If you have trouble, use an AVR programmer.

## Licence

The Arduino libraries are licensed under the GPL, and so this project is also licensed under the GPL (v3 or later version, at your choice).

See the [LICENCE](./LICENCE) file.

[HID over I²C]: http://msdn.microsoft.com/en-us/library/windows/hardware/hh852380.aspx
[HID over USB]: https://www.usb.org/sites/default/files/hid1_11.pdf
[Arduino]: https://www.arduino.cc
[miniCore]: https://github.com/MCUdude/MiniCore
[ArduinoISP sketch]: https://www.arduino.cc/en/Tutorial/ArduinoISP
[Atmel-ICE]: https://www.microchip.com/Developmenttools/ProductDetails/ATATMEL-ICE