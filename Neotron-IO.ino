/**
 * Neotron-IO Firmware.
 *
 * Uses MiniCore for the AtMega328P. Set to 8 MHz Internal RC. Don't go higher than 9600 baud.
 *
 * This project is also licensed under the GPL (v3 or later version, at your choice).
 * See the [LICENCE](./LICENCE) file.
 */

//
// Includes
//

#include <avr/io.h>
#include <Arduino.h>
#include <EEPROM.h>

#include "joystick.h"

//
// Constants
//

const int JS2_PIN_UP = 21;
const int JS2_PIN_AB = 20;
const int JS2_PIN_DOWN = 13;
const int JS2_PIN_GND_LEFT = 12;
const int JS2_PIN_GND_RIGHT = 11;
const int JS2_PIN_START_C = 10;

const int CALIBRATION_ON = 8;
const int CALIBRATION_OUT = 9;

const int JS1_PIN_UP = 7;
const int JS1_PIN_AB = 6;
const int JS1_PIN_DOWN = 5;
const int JS1_PIN_GND_LEFT = 4;
const int JS1_PIN_GND_RIGHT = 3;
const int JS1_PIN_START_C = 2;
// Pins D0 and D1 are for the UART

const int JS1_PIN_SELECT = A5;
const int JS2_PIN_SELECT = A4;
// A3 is MS_DAT
// A2 is MS_CLK
// A1 is KB_DAT
// A0 is KB_CLK

const uint8_t EEPROM_MAGIC_BYTE = 0xE0;
const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_OSCCAL = 1;

const uint16_t DEBOUNCE_LOOPS = 5;

//
// Variables
//

static Joystick js1(JS1_PIN_UP, JS1_PIN_DOWN, JS1_PIN_GND_LEFT, JS1_PIN_GND_RIGHT, JS1_PIN_AB, JS1_PIN_START_C, JS1_PIN_SELECT);
static Joystick js2(JS2_PIN_UP, JS2_PIN_DOWN, JS2_PIN_GND_LEFT, JS2_PIN_GND_RIGHT, JS2_PIN_AB, JS2_PIN_START_C, JS2_PIN_SELECT);

static bool calibration_mode = 0;

//
// Functions
//

// the setup function runs once when you press reset or power the board
void setup() {
	if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC_BYTE) {
		// Load OSCCAL
		OSCCAL = EEPROM.read(EEPROM_ADDR_OSCCAL);
	}

	// One of the few 'standard' baud rates you can easily hit from an 8 MHz
	// clock
	Serial.begin(9600);
	// Sign-on banner
	Serial.println("B:Neotron-IO v0.1.1 starting");
	pinMode(CALIBRATION_ON, INPUT_PULLUP);
	pinMode(CALIBRATION_OUT, OUTPUT);
	// Pull this pin low on reset to enter calibration mode
	calibration_mode = (digitalRead(CALIBRATION_ON) == 0);
	if (calibration_mode) {
		// The frequency of this output should be 8 MHz / (256 * 64 * 2), or
		// 244.14 Hz. You need to adjust OSCCAL until you get within 5% for a
		// functioning UART - the closer the better as it drifts over
		// temperature.
		analogWrite(CALIBRATION_OUT, 128);
	}
}

// the loop function runs over and over again forever
void loop() {
	static uint16_t debounce_count = 0;
	JoystickResult js1_bits;
	JoystickResult js2_bits;

	// Don't do this if keyboard read in progress
	if (js1.scan()) {
		js1_bits = js1.read();
		Serial.print(F("1:"));
		Serial.println(js1_bits.value(), HEX);
	}

	// Don't do this if keyboard read in progress
	if (js2.scan()) {
		js2_bits = js2.read();
		Serial.print(F("2:"));
		Serial.println(js2_bits.value(), HEX);
	}

	if (calibration_mode) {
		// Look at joystick and trim OSCCAL up or down
		if (js1_bits.is_a_pressed()) {
			// Has the A button been down for enough time?
			if (debounce_count == DEBOUNCE_LOOPS) {
				// Yes it has. If the are pressing up, increase OSCCAL.
				// If they are pressing down, decrease OSCCAL.
				if (js1_bits.is_up_pressed()) {
					OSCCAL++;
				} else if (js1_bits.is_down_pressed()) {
					OSCCAL--;
				}
				debounce_count++;
			} else if (debounce_count < DEBOUNCE_LOOPS) {
				debounce_count++;
			}
		} else {
			debounce_count = 0;
		}
		if (js1_bits.is_start_pressed()) {
			EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_BYTE);
			EEPROM.write(EEPROM_ADDR_OSCCAL, OSCCAL);
			while(1) {
				// Lock up once we've saved the EEPROM
				Serial.println("RESET ME");
			}
		}
		// Whatever happens, print out the current OSCCAL
		Serial.print("OSC:");
		Serial.println(OSCCAL, HEX);
	}

}
