/*
 *  Neotron-IO Firmware.
 *
 *  Uses MiniCore for the AtMega328P. Set to 1 MHz Internal RC. Don't go higher than 9600 baud.
 */

#include <avr/io.h>
#include <Arduino.h>
#include <EEPROM.h>

class Joystick {
public:
	Joystick(
		int pin_up,
		int pin_down,
		int pin_gnd_left,
		int pin_gnd_right,
		int pin_a_b,
		int pin_start_c,
		int pin_select
	):
		m_pin_up(pin_up),
		m_pin_down(pin_down),
		m_pin_gnd_left(pin_gnd_left),
		m_pin_gnd_right(pin_gnd_right),
		m_pin_a_b(pin_a_b),
		m_pin_start_c(pin_start_c),
		m_pin_select(pin_select),
		m_pinmap_old(0),
		m_pinmap(0)
	{
		pinMode(m_pin_up, INPUT_PULLUP);
		pinMode(m_pin_down, INPUT_PULLUP);
		pinMode(m_pin_gnd_left, INPUT_PULLUP);
		pinMode(m_pin_gnd_right, INPUT_PULLUP);
		pinMode(m_pin_a_b, INPUT_PULLUP);
		pinMode(m_pin_start_c, INPUT_PULLUP);
		pinMode(m_pin_select, OUTPUT);
		digitalWrite(m_pin_select, 0);
	}

	/// Get the current joystick state
	uint16_t read() {
		m_pinmap_old = m_pinmap;
		return m_pinmap;
	}

	/// @return true if value different to the last call to `read`, otherwise false
	bool has_new() const {
		return (m_pinmap != m_pinmap_old);
	}

	///
	/// Scans the joystick pins and calculates the new value.
	///
	/// Stored internally until you call `read`.
	///
	/// @return true if value different to the last call to `read`, otherwise false
	///
	bool scan() {
		uint16_t new_pinmap = 0;
		// Pins are active low
		if (digitalRead(m_pin_up) == 0) {
			new_pinmap |=  1 << SHIFT_UP;
		};
		if (digitalRead(m_pin_down) == 0) {
			new_pinmap |=  1 << SHIFT_DOWN;
		};
		if (digitalRead(m_pin_a_b) == 0) {
			new_pinmap |=  1 << SHIFT_A;
		};
		if (digitalRead(m_pin_start_c) == 0) {
			new_pinmap |=  1 << SHIFT_START;
		};
		if(digitalRead(m_pin_gnd_left) == 0) {
			new_pinmap |= 1 << SHIFT_LEFT;
		};
		if(digitalRead(m_pin_gnd_right) == 0) {
			new_pinmap |= 1 << SHIFT_RIGHT;
		};
		if ( ( new_pinmap & MASK_LEFT_RIGHT ) == MASK_LEFT_RIGHT )
		{
			// Impossible for left and right to be active at the same time, so
			// we must have a SEGA MegaDrive pad.
			digitalWrite(m_pin_select, 1);
			// Clear the left/right pins as they aren't actually set
			new_pinmap &= ~MASK_LEFT_RIGHT;
			// Read the alternative pins
			if (digitalRead(m_pin_gnd_left) == 0) {
				new_pinmap |= 1 << SHIFT_LEFT;
			}
			if (digitalRead(m_pin_gnd_right) == 0) {
				new_pinmap |= 1 << SHIFT_RIGHT;
			}
			if (digitalRead(m_pin_a_b) == 0) {
				new_pinmap |= 1 << SHIFT_B;
			}
			if (digitalRead(m_pin_start_c) == 0) {
				new_pinmap |= 1 << SHIFT_C;
			}
			// Turn select off again
			digitalWrite(m_pin_select, 0);
		}
		m_pinmap = new_pinmap;
		return has_new();
	}

	static constexpr uint8_t SHIFT_UP = 0;
	static constexpr uint8_t SHIFT_DOWN = 1;
	static constexpr uint8_t SHIFT_LEFT = 2;
	static constexpr uint8_t SHIFT_RIGHT = 3;
	static constexpr uint8_t SHIFT_A = 4;
	static constexpr uint8_t SHIFT_B = 5;
	static constexpr uint8_t SHIFT_C = 6;
	static constexpr uint8_t SHIFT_START = 7;
	static constexpr uint8_t MASK_LEFT_RIGHT = ( 1 << SHIFT_LEFT ) | ( 1 << SHIFT_RIGHT );

private:
	uint16_t m_pinmap;
	uint16_t m_pinmap_old;

	const int m_pin_start_c;
	const int m_pin_a_b;
	const int m_pin_down;
	const int m_pin_up;
	const int m_pin_gnd_right;
	const int m_pin_gnd_left;
	const int m_pin_select;
};

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

static Joystick js1(JS1_PIN_UP, JS1_PIN_DOWN, JS1_PIN_GND_LEFT, JS1_PIN_GND_RIGHT, JS1_PIN_AB, JS1_PIN_START_C, JS1_PIN_SELECT);
static Joystick js2(JS2_PIN_UP, JS2_PIN_DOWN, JS2_PIN_GND_LEFT, JS2_PIN_GND_RIGHT, JS2_PIN_AB, JS2_PIN_START_C, JS2_PIN_SELECT);

static bool calibration_mode = 0;
const uint16_t DEBOUNCE_LOOPS = 5;

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
	Serial.println("B:Neotron-IO v0.1.0 starting");
	pinMode(CALIBRATION_ON, INPUT_PULLUP);
	pinMode(CALIBRATION_OUT, OUTPUT);
	// Pull this pin low on reset to enter calibration mode
	calibration_mode = (digitalRead(CALIBRATION_ON) == 0);
	if (calibration_mode) {
		// The frequency of this output should be 8 MHz / (256 * 64 * 2), or
		// 244.14 Hz. +/- 2% on that is 239.25 Hz to 249.02 Hz. If you are in that
		// range, the UART should work fine.
		analogWrite(CALIBRATION_OUT, 128);
	}
}

// the loop function runs over and over again forever
void loop() {
	static uint16_t debounce_count = 0;
	uint16_t js1_bits = 0;
	uint16_t js2_bits = 0;

	if (js1.scan()) {
		js1_bits = js1.read();
		Serial.print(F("1:"));
		Serial.println(js1_bits, HEX);
	}

	if (js2.scan()) {
		js2_bits = js2.read();
		Serial.print(F("1:"));
		Serial.println(js2_bits, HEX);
	}

	if (calibration_mode) {
		// Look at joystick and trim OSCCAL up or down
		if (js1_bits & (1 << Joystick::SHIFT_A)) {
			// Has the A button been down for enough time?
			if (debounce_count == DEBOUNCE_LOOPS) {
				// Yes it has. If the are pressing up, increase OSCCAL.
				// If they are pressing down, decrease OSCCAL.
				if (js1_bits & (1 << Joystick::SHIFT_UP)) {
					OSCCAL++;
				} else if (js1_bits & (1 << Joystick::SHIFT_DOWN)) {
					OSCCAL--;
				}
				debounce_count++;
			} else if (debounce_count < DEBOUNCE_LOOPS) {
				debounce_count++;
			}
		} else {
			debounce_count = 0;
		}
		if (js1_bits & (1 << Joystick::SHIFT_START)) {
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
