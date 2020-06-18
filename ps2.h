/**
 * Neotron-IO Atari/Sega PS/2 driver.
 *
 * Reads bytes from a generic PS/2 device. Can also write bytes.
 *
 * If this device is active, then poll it as fast as possible and don't do
 * anything else otherwise you might miss an edge. PS/2 clock is driven by the
 * keyboard/mouse device at around 10 kHz (so 0.1ms or 100us or 800 CPU clock
 * cycles per clock).
 *
 * This driver is for generic PS/2 devices and doesn't understand the
 * difference between a keyboard and a mouse.
 *
 * This project is also licensed under the GPL (v3 or later version, at your choice).
 * See the [LICENCE](./LICENCE) file.
 */

enum State {
	Idle,
	Active,
	BufferFull,
	Disabled
};

/**
 * Represents a generic PS2 device.
 */
class Ps2 {
public:
	Ps2(
		int pin_clk,
		int pin_data
	):
		m_pin_clk(pin_clk),
		m_pin_data(pin_data),
		m_state(State::Idle),
		m_last_clk(true),
		m_bitcollector(0),
		m_num_bits(0),
		m_timeout(TIMEOUT_POLLS),
		m_buffer_used(0)
	{
		// Use internal pull-ups
		pinMode(m_pin_clk, INPUT_PULLUP);
		pinMode(m_pin_data, INPUT_PULLUP);
	}

	bool is_active() {
		return (m_state == State::Active);
	}

	void poll() {
		switch (m_state) {
		case State::Idle:
			poll_idle();
			break;
		case State::BufferFull:
		case State::Disabled:
			break;
		case State::Active:
			poll_active();
			break;
		}
	}

	/**
	 * Disables the PS/2 port by holding the clock line low.
	 */
	void disable() {
		pinMode(m_pin_clk, OUTPUT);
		digitalWrite(m_pin_clk, LOW);
		m_state = State::Disabled;
	}

	/**
	 * Re-enables the PS/2 port by releasing the clock line.
	 */
	void renable() {
		pinMode(m_pin_clk, INPUT_PULLUP);
		m_state = State::Idle;
		m_bitcollector = 0;
		m_num_bits = 0;
	}

	/**
	 * Get a byte from the keyboard buffer. Returns -1 if the buffer is empty.
	 */
	int read_buffer() {
		if (m_buffer_used == 0) {
			return -1;
		} else {
			m_buffer_used--;
			if (m_state == State::BufferFull) {
				renable();
			}
			return m_buffer[m_buffer_used];
		}
	}

#ifndef TEST_MODE_NO_PRIVATE
private:
#endif

	void poll_idle() {
		bool kb_clk_pin = digitalRead(m_pin_clk);
		if (kb_clk_pin != m_last_clk) {
			// We have an edge
			poll_active();
		}
	}

	void poll_active() {
		bool kb_clk_pin = digitalRead(m_pin_clk);
		if (kb_clk_pin != m_last_clk) {
			// Edge
			if (!kb_clk_pin)
			{
				// Falling edge
				if (digitalRead(m_pin_data))
				{
					m_bitcollector |= (1 << m_num_bits);
				}
				m_num_bits++;
				if (m_num_bits == BITS_IN_PS2_WORD)
				{
					int result = validate_word(m_bitcollector);
					if (result >= 0) {
						m_buffer[m_buffer_used++] = (uint8_t) result;
						if (m_buffer_used == BUFFER_SIZE) {
							disable();
							m_state = State::BufferFull;
						}
					}
				}
			}
			m_timeout = TIMEOUT_POLLS;
			m_last_clk = kb_clk_pin;				
		} else {
			m_timeout--;
			if (m_timeout == 0) {
				// Hmm ... keyboard stopped part way through?
				m_state = State::Idle;
				m_num_bits = 0;
				m_bitcollector = 0;
			}
		}
	}

	static int validate_word(uint16_t ps2_bits) {
		bool parity_bit = (ps2_bits & (1 << PARITY_BIT)) != 0;
		bool start_bit = (ps2_bits & (1 << START_BIT)) != 0;
		bool stop_bit = (ps2_bits & (1 << STOP_BIT)) != 0;
		bool parity = parity_bit;
		for(uint8_t i = FIRST_DATA_BIT; i <= LAST_DATA_BIT; i++) {
			if (ps2_bits & (1 << i))
			{
				parity = !parity;
			}
		}
		if (!start_bit && parity && stop_bit) {
			return (ps2_bits >> 1) & 0x00FF;
		} else {
			return -1;
		}
	}

	static constexpr uint8_t PARITY_BIT = 9;
	static constexpr uint8_t STOP_BIT = 10;
	static constexpr uint8_t START_BIT = 0;
	static constexpr uint8_t FIRST_DATA_BIT = 1;
	static constexpr uint8_t LAST_DATA_BIT = 8;
	static constexpr size_t BUFFER_SIZE = 32;
	static constexpr size_t BITS_IN_PS2_WORD = 11;
	static constexpr uint16_t TIMEOUT_POLLS = 800;

	State m_state;
	int m_pin_clk;
	int m_pin_data;
	bool m_last_clk;
	uint16_t m_bitcollector;
	size_t m_num_bits;
	uint8_t m_buffer[BUFFER_SIZE];
	size_t m_buffer_used;
	uint16_t m_timeout;
};
