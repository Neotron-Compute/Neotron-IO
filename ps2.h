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

#include <cstring>
#include <assert.h>

enum class Ps2State
{
	Idle,
	ReadingWord,
	WritingWord,
	BufferFull,
	Disabled
};

enum class Ps2WriteState {
	HoldingClock,
	WaitForClock,
	WaitClockHigh,
	WaitClockLow,
};

/**
 * Represents a generic PS2 device.
 */
class Ps2
{
public:
	/**
	 * Construct a new Ps2 object.
	 * 
	 * @param pin_clk the Arduino pin number for the PS/2 clock pin
	 * @param pin_data the Arduino pin number for the PS/2 data pin
	 */
	Ps2(
		int pin_clk,
		int pin_data) : m_pin_clk(pin_clk),
						m_pin_data(pin_data),
						m_state(Ps2State::Idle),
						m_write_state(Ps2WriteState::HoldingClock),
						m_last_clk(true),
						m_current_word(0),
						m_current_word_num_bits(0),
						m_timeout(0),
						m_in_buffer_used(0),
						m_out_buffer_used(0)
	{
		// Use internal pull-ups
		pinMode(m_pin_clk, INPUT_PULLUP);
		pinMode(m_pin_data, INPUT_PULLUP);
	}

	/**
	 * Are we currently talking to the PS/2 device (either reading or writing a word)?
	 */
	bool isActive()
	{
		return (m_state == Ps2State::ReadingWord) || (m_state == Ps2State::WritingWord);
	}

	/**
	 * Checks the clock pin and reads a bit as required.
	 * 
	 * You must call this often to avoid missing clock pulse edges.
	 */
	void poll()
	{
		switch (m_state)
		{
		case Ps2State::Idle:
			pollIdle();
			break;
		case Ps2State::BufferFull:
		case Ps2State::Disabled:
			break;
		case Ps2State::WritingWord:
			pollWritingWord();
			break;
		case Ps2State::ReadingWord:
			pollReadingWord();
			break;
		}
	}

	/**
	 * Disables the PS/2 port by holding the clock line low.
	 */
	void disable()
	{
		pinMode(m_pin_clk, OUTPUT);
		digitalWrite(m_pin_clk, LOW);
		m_state = Ps2State::Disabled;
	}

	/**
	 * Re-enables the PS/2 port by releasing the clock line.
	 */
	void renable()
	{
		pinMode(m_pin_clk, INPUT_PULLUP);
		m_state = Ps2State::Idle;
		m_current_word = 0;
		m_current_word_num_bits = 0;
	}

	/**
	 * Write data to the PS/2 device.
	 * 
	 * The data is buffered and will be clocked out in subsequent calls to `poll`.
	 * 
	 * @param data the bytes to write to the device
	 * @param data_len the number of bytes pointed to by `data`
	 * 
	 * @return true if space in internal buffer to accept all the bytes, false if not enough space and write is rejected.
	 */
	bool writeBuffer(const uint8_t *data, size_t data_len)
	{
		if ((data_len + m_out_buffer_used) > OUT_BUFFER_SIZE)
		{
			return false;
		}
		memcpy(m_out_buffer + m_out_buffer_used, data, data_len);
		m_out_buffer_used += data_len;
	}

	/**
	 * Get a byte from the keyboard buffer. Returns -1 if the buffer is empty.
	 */
	int readBuffer()
	{
		if (m_in_buffer_used == 0)
		{
			return -1;
		}
		else
		{
			m_in_buffer_used--;
			if (m_state == Ps2State::BufferFull)
			{
				renable();
			}
			return m_in_buffer[m_in_buffer_used];
		}
	}

#ifndef TEST_MODE_NO_PRIVATE
private:
#endif

	void pollIdle()
	{
		if (m_out_buffer_used > 0)
		{
			// We are idle and we have words waiting to clock out
			m_state = Ps2State::WritingWord;
			m_write_state = Ps2WriteState::HoldingClock;
			// Hold clock line - set as output
			digitalWrite(m_pin_clk, LOW);
			pinMode(m_pin_clk, OUTPUT);
			setTimeout(150);
		}

		bool kb_clk_pin = digitalRead(m_pin_clk);
		if (kb_clk_pin != m_last_clk)
		{
			// We have an edge
			m_state = Ps2State::ReadingWord;
		}
	}

	void pollWritingWord()
	{
		switch (m_write_state) {
		case Ps2WriteState::HoldingClock:
			if (hasTimedOut()) {
				m_write_state = Ps2WriteState::WaitForClock;
				// Now hold data line - set as output
				digitalWrite(m_pin_data, LOW);
				pinMode(m_pin_data, OUTPUT);
				// Then release clock line - set back as input
				pinMode(m_pin_clk, INPUT_PULLUP);
			}
			break;
		case Ps2WriteState::WaitForClock:
			if (digitalRead(m_pin_clk) == LOW) {

			}
			break;
		case Ps2WriteState::WaitClockHigh:
			break;
		case Ps2WriteState::WaitClockLow:
			break;
		}
	}

	void pollReadingWord()
	{
		bool kb_clk_pin = digitalRead(m_pin_clk);
		if (kb_clk_pin != m_last_clk)
		{
			// Edge
			if (!kb_clk_pin)
			{
				// Falling edge
				if (digitalRead(m_pin_data))
				{
					m_current_word |= (1 << m_current_word_num_bits);
				}
				m_current_word_num_bits++;
				if (m_current_word_num_bits == BITS_IN_PS2_WORD)
				{
					int result = validateWord(m_current_word);
					if (result >= 0)
					{
						m_in_buffer[m_in_buffer_used++] = (uint8_t)result;
						if (m_in_buffer_used == IN_BUFFER_SIZE)
						{
							disable();
							m_state = Ps2State::BufferFull;
						}
					}
				}
			}
			setTimeout(250);
			m_last_clk = kb_clk_pin;
		}
		else
		{
			if (hasTimedOut())
			{
				// Hmm ... keyboard stopped part way through for 1..2ms?
				// Give up.
				m_state = Ps2State::Idle;
				m_current_word_num_bits = 0;
				m_current_word = 0;
			}
		}
	}

	/**
	 * Waits for num_ms to 1+num_micros microseconds.
	 */
	void setTimeout(uint16_t num_micros) {
		assert(num_micros < 32000);
		m_timeout = ((uint16_t) (micros() & 0xFFFF)) + num_micros;
	}

	/**
	 * Has the timeout expired?
	 */
	bool hasTimedOut() {
		uint16_t now = micros() & 0xFFFF;
		int16_t delta = m_timeout - now;
		if (delta <= 0) {
			m_timeout = 0;
			return true;
		} else {
			return false;
		}
	}

	static uint16_t encodeByte(uint8_t byte) {
		uint16_t result = 0;
		bool parity = true;
		for(size_t i = 0; i < 8; i++) {
			if (bitRead(byte, i)) {
				parity = !parity;
			}
		}
		// Data byte (first bit is start bit, which is zero)
		result |= ((uint16_t) byte) << 1;
		// Parity bit
		result |= (parity ? 1 : 0) << 9;
		// Stop bit
		result |= 1 << 10;
		return result;
	}

	/**
	 * Check an 11-bit word from the device.
	 */
	static int validateWord(uint16_t ps2_bits)
	{
		bool parity_bit = bitRead(ps2_bits, PARITY_BIT) != 0;
		bool start_bit = bitRead(ps2_bits, START_BIT) != 0;
		bool stop_bit = bitRead(ps2_bits, STOP_BIT) != 0;
		bool parity = parity_bit;
		for (uint8_t i = FIRST_DATA_BIT; i <= LAST_DATA_BIT; i++)
		{
			if (bitRead(ps2_bits, i))
			{
				parity = !parity;
			}
		}
		if (!start_bit && parity && stop_bit)
		{
			return (ps2_bits >> 1) & 0x00FF;
		}
		else
		{
			return -1;
		}
	}

	static constexpr uint8_t PARITY_BIT = 9;
	static constexpr uint8_t STOP_BIT = 10;
	static constexpr uint8_t START_BIT = 0;
	static constexpr uint8_t FIRST_DATA_BIT = 1;
	static constexpr uint8_t LAST_DATA_BIT = 8;
	static constexpr size_t IN_BUFFER_SIZE = 32;
	static constexpr size_t OUT_BUFFER_SIZE = 32;
	static constexpr size_t BITS_IN_PS2_WORD = 11;
	static constexpr uint16_t TIMEOUT_POLLS = 800;

	Ps2State m_state;
	Ps2WriteState m_write_state;
	int m_pin_clk;
	int m_pin_data;
	bool m_last_clk;
	uint16_t m_current_word;
	size_t m_current_word_num_bits;
	uint8_t m_in_buffer[IN_BUFFER_SIZE];
	size_t m_in_buffer_used;
	uint8_t m_out_buffer[OUT_BUFFER_SIZE];
	size_t m_out_buffer_used;
	uint16_t m_timeout;
};
