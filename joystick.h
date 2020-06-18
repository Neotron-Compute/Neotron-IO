/**
 * Neotron-IO Atari/Sega Joystick driver.
 *
 * Can read from standard single (fire) button 9-pin Atari / Sega Master System
 * controllers, and from three (ABC) button 9-pin Sega Mega Drive / Genesis controllers.
 *
 * The latter are distinguished by presenting both left and right active
 * together (which is impossible normally). This tells us to flip the SELECT
 * line and read again to get the other set of buttons. Note that six fire
 * button Sega Mega Drive / Genesis pads are not supported, and will be read
 * like three button pads.
 *
 * This project is also licensed under the GPL (v3 or later version, at your choice).
 * See the [LICENCE](./LICENCE) file.
 */

/**
 * Represents a reading from a Joystick port.
 */
class JoystickResult {
public:
	JoystickResult():
		data(0)
	{
		// Nothing here
	}

	JoystickResult(
		uint16_t bits
	):
		data(bits)
	{
		// Nothing here
	}

	void set_fire_pressed() {
		data |= (1 << SHIFT_A);
	}

	void set_a_pressed() {
		data |= (1 << SHIFT_A);
	}

	void set_b_pressed() {
		data |= (1 << SHIFT_B);
	}

	void set_c_pressed() {
		data |= (1 << SHIFT_C);
	}

	void set_up_pressed() {
		data |= (1 << SHIFT_UP);
	}

	void set_down_pressed() {
		data |= (1 << SHIFT_DOWN);
	}

	void set_left_pressed() {
		data |= (1 << SHIFT_LEFT);
	}

	void set_right_pressed() {
		data |= (1 << SHIFT_RIGHT);
	}

	void set_start_pressed() {
		data |= (1 << SHIFT_START);
	}

	bool is_fire_pressed() const {
		return (data & (1 << SHIFT_A)) != 0;
	}

	bool is_a_pressed() const {
		return (data & (1 << SHIFT_A)) != 0;
	}

	bool is_b_pressed() const {
		return (data & (1 << SHIFT_B)) != 0;
	}

	bool is_c_pressed() const {
		return (data & (1 << SHIFT_C)) != 0;
	}

	bool is_up_pressed() const {
		return (data & (1 << SHIFT_UP)) != 0;
	}

	bool is_down_pressed() const {
		return (data & (1 << SHIFT_DOWN)) != 0;
	}

	bool is_left_pressed() const {
		return (data & (1 << SHIFT_LEFT)) != 0;
	}

	bool is_right_pressed() const {
		return (data & (1 << SHIFT_RIGHT)) != 0;
	}

	bool is_start_pressed() const {
		return (data & (1 << SHIFT_START)) != 0;
	}

	bool is_left_right_pressed() const {
		return is_left_pressed() && is_right_pressed();
	}

	void clear_left_right_pressed() {
		data &= ~(1 << SHIFT_LEFT);
		data &= ~(1 << SHIFT_RIGHT);
	}

	uint16_t value() {
		return data;
	}

	bool operator==(const JoystickResult& rhs) const {
		return data == rhs.data;
	}

	bool operator!=(const JoystickResult& rhs) const {
		return data != rhs.data;
	}

private:
	uint16_t data;

	static constexpr uint8_t SHIFT_UP = 0;
	static constexpr uint8_t SHIFT_DOWN = 1;
	static constexpr uint8_t SHIFT_LEFT = 2;
	static constexpr uint8_t SHIFT_RIGHT = 3;
	static constexpr uint8_t SHIFT_A = 4;
	static constexpr uint8_t SHIFT_B = 5;
	static constexpr uint8_t SHIFT_C = 6;
	static constexpr uint8_t SHIFT_START = 7;
	static constexpr uint8_t MASK_LEFT_RIGHT = ( 1 << SHIFT_LEFT ) | ( 1 << SHIFT_RIGHT );
};

/**
 * Represents a Joystick port that you can read.
 */
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
	JoystickResult read() {
		m_pinmap_old = m_pinmap;
		return JoystickResult(m_pinmap);
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
		JoystickResult new_pinmap;
		// Pins are active low
		if (digitalRead(m_pin_up) == 0) {
			new_pinmap.set_up_pressed();
		};
		if (digitalRead(m_pin_down) == 0) {
			new_pinmap.set_down_pressed();
		};
		if (digitalRead(m_pin_a_b) == 0) {
			new_pinmap.set_a_pressed();
		};
		if (digitalRead(m_pin_start_c) == 0) {
			new_pinmap.set_start_pressed();
		};
		if(digitalRead(m_pin_gnd_left) == 0) {
			new_pinmap.set_left_pressed();
		};
		if(digitalRead(m_pin_gnd_right) == 0) {
			new_pinmap.set_right_pressed();
		};
		if ( new_pinmap.is_left_right_pressed() )
		{
			// Impossible for left and right to be active at the same time, so
			// we must have a SEGA MegaDrive pad.
			digitalWrite(m_pin_select, 1);
			// Clear the left/right pins as they aren't actually set
			new_pinmap.clear_left_right_pressed();
			// Read the alternative pins
			if (digitalRead(m_pin_gnd_left) == 0) {
				new_pinmap.set_left_pressed();
			}
			if (digitalRead(m_pin_gnd_right) == 0) {
				new_pinmap.set_right_pressed();
			}
			if (digitalRead(m_pin_a_b) == 0) {
				new_pinmap.set_b_pressed();
			}
			if (digitalRead(m_pin_start_c) == 0) {
				new_pinmap.set_c_pressed();
			}
			// Turn select off again
			digitalWrite(m_pin_select, 0);
		}
		m_pinmap = new_pinmap;
		return has_new();
	}

private:
	JoystickResult m_pinmap;
	JoystickResult m_pinmap_old;

	const int m_pin_start_c;
	const int m_pin_a_b;
	const int m_pin_down;
	const int m_pin_up;
	const int m_pin_gnd_right;
	const int m_pin_gnd_left;
	const int m_pin_select;
};
