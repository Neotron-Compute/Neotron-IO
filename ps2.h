/**
 * Neotron-IO PS/2 interface driver.
 *
 * Reads bytes from a generic PS/2 device. Can also write bytes.
 *
 * This driver is for generic PS/2 devices and doesn't understand the
 * difference between a keyboard and a mouse.
 *
 * This project is also licensed under the GPL (v3 or later version, at your
 * choice). See the [LICENCE](./LICENCE) file.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "RingBuf.h"

/**
 * Represents a generic PS2 device.
 *
 * All I/O is handled externally.
 */
class Ps2
{
   public:
	enum class Edge
	{
		EDGE_RISING,
		EDGE_FALLING
	};

	enum class Level
	{
		LEVEL_LOW,
		LEVEL_HIGH,
	};

	typedef void ( *SetClockInputFn )( void );
	typedef void ( *SetClockOutputFn )( Level level );
	typedef void ( *SetDataInputFn )( void );
	typedef void ( *SetDataOutputFn )( Level level );

   private:
	enum class State
	{
		Idle,
		ReadingWord,
		Disabled
	};

	enum class WriteState
	{
		HoldingClock,
		WaitClockLow,
		WaitClockHigh,
		WaitDataLow,
		WaitFinalClockLow,
		WaitForRelease
	};

	static constexpr uint8_t PARITY_BIT = 9;
	static constexpr uint8_t STOP_BIT = 10;
	static constexpr uint8_t START_BIT = 0;
	static constexpr uint8_t FIRST_DATA_BIT = 1;
	static constexpr uint8_t LAST_DATA_BIT = 8;
	static constexpr size_t BUFFER_SIZE = 32;
	static constexpr uint16_t PS2_INCOMING_MASK = 1 << 11;
	static constexpr uint16_t PS2_OUTGOING_MASK = 1 << 10;
	static constexpr uint16_t TIMEOUT_POLLS = 800;

	volatile State m_state;
	Level m_last_clk;
	uint16_t m_current_word;
	uint16_t m_current_word_bitmask;
	int m_valid_word;
	uint16_t m_timeout_count;
	bool m_have_timeout;
	SetClockInputFn m_set_clock_input_fn;
	SetClockOutputFn m_set_clock_output_fn;
	SetDataInputFn m_set_data_input_fn;
	SetDataOutputFn m_set_data_output_fn;

   public:
	/**
	 * Construct a new Ps2 object.
	 */
	Ps2( SetClockInputFn _set_clock_input_fn,
	     SetClockOutputFn _set_clock_output_fn,
	     SetDataInputFn _set_data_input_fn,
	     SetDataOutputFn _set_data_output_fn

	     )
	    : m_state( State::Idle ),
	      m_set_clock_input_fn( _set_clock_input_fn ),
	      m_set_clock_output_fn( _set_clock_output_fn ),
	      m_set_data_input_fn( _set_data_input_fn ),
	      m_set_data_output_fn( _set_data_output_fn ),
	      m_current_word( 0 ),
	      m_current_word_bitmask( 1 ),
	      m_valid_word( -1 ),
	      m_timeout_count( 0 ),
	      m_have_timeout( false )
	{
	}

	/**
	 * Are we currently talking to the PS/2 device (either reading or writing a
	 * word)?
	 */
	bool isActive() { return ( m_state == State::ReadingWord ); }

	/**
	 * Call this on a falling edge of the clock signal
	 */
	void clockEdge( Edge edge, Level data_bit )
	{
		switch ( m_state )
		{
			case State::Idle:
				handleClockEdgeIdle( edge, data_bit );
				break;
			case State::Disabled:
				// Should never happen!
				break;
			case State::ReadingWord:
				handleClockEdgeReadingWord( edge, data_bit );
				break;
		}
	}

	/**
	 * Checks for timeouts. Call this in your main loop.
	 *
	 * @return -1 if no data available, otherwise the most recently received
	 * data byte
	 */
	int poll( void )
	{
		if ( m_have_timeout )
		{
			if ( m_timeout_count > 0 )
			{
				m_timeout_count -= 1;
			}
		}
		switch ( m_state )
		{
			case State::Idle:
				break;
			case State::Disabled:
				break;
			case State::ReadingWord:
				// Check for timeouts
				handlePollReadingWord();
				break;
		}
		noInterrupts();
		int result = m_valid_word;
		m_valid_word = -1;
		interrupts();
		return result;
	}

	/**
	 * Disables the PS/2 port by holding the clock line low.
	 *
	 * I don't suggest you do this while clocking out a byte, unless you want
	 * to abort. Make sure you're in Idle first.
	 */
	void disable()
	{
		noInterrupts();
		m_set_clock_output_fn( Level::LEVEL_LOW );
		m_state = State::Disabled;
		interrupts();
	}

	/**
	 * Re-enables the PS/2 port by releasing the clock line.
	 *
	 * Also resets our system state.
	 */
	void enable()
	{
		noInterrupts();
		m_set_clock_input_fn();
		m_set_data_input_fn();
		m_state = State::Idle;
		interrupts();
	}

	/**
	 * Write data to the PS/2 device.
	 *
	 * This function isn't called very often (only when you want to change a
	 * Keyboard light, or re-configure a Mouse), so we busy-wait until the
	 * data is sent. Make sure you disable any other PS/2 devices before you
	 * call this function.
	 *
	 * @param data_byte the byte to write to the device
	 *
	 * @return false if the remote device rejects our byte (you should send it
	 *     again) or true if it was sent OK.
	 */
	bool sendByte( uint8_t data_byte )
	{
		// TODO send data here!
		return false;
	}

	/// Debug function - gets internal state
	uint16_t getState()
	{
		uint16_t result = 0;
		result |= static_cast<uint8_t>( m_state );
		return result;
	}

#ifndef TEST_MODE_NO_PRIVATE
   private:
#endif

	/// Handle an IRQ when we are in the idle state.
	///
	/// This means we have a new byte coming in.
	void handleClockEdgeIdle( Edge edge, Level data_bit )
	{
		if ( edge == Edge::EDGE_FALLING )
		{
			// We have a falling edge so store this bit and set things up
			m_current_word_bitmask = 2;
			m_current_word = data_bit == Level::LEVEL_HIGH ? 1 : 0;
			// We're now reading a word, so record this in `m_state`
			m_state = State::ReadingWord;
		}
	}

#if 0
	/**
	 * Handle a clock edge whilst writing out data.
	 */
	void irqWritingWord()
	{
		switch ( m_write_state )
		{
			case WriteState::HoldingClock:
				if ( hasTimedOut() )
				{
					m_write_state = WriteState::WaitClockLow;
					// 2) Bring the Data line low.
					m_set_data_output( Level::LEVEL_LOW );
					// 3) Release the Clock line.
					m_set_clock_input_fn();
				}
				break;
			// 4) Wait for the device to bring the clock line low
			//    (this is when we set the data line).
			case WriteState::WaitClockLow:
				if ( fastDigitalRead( PIN_CLK ) == LOW )
				{
					// Are we done?
					if ( m_current_word_bitmask == PS2_OUTGOING_MASK )
					{
						// 9) All data + parity clocked out - time to Release
						// data line
						m_set_data_input_fn();
						m_write_state = WriteState::WaitDataLow;
						setExternalTimeout( 1500 );
					}
					else
					{
						// 5) Set/reset the data pin according to the next bit
						if ( m_current_word & m_current_word_bitmask )
						{
							fastDigitalWrite( PIN_DAT, HIGH );
						}
						else
						{
							fastDigitalWrite( PIN_DAT, LOW );
						}
						m_current_word_bitmask <<= 1;
						m_write_state = WriteState::WaitClockHigh;
						setExternalTimeout( 1500 );
					}
				}
				break;
			// 6) Wait for device to bring the clock line high
			//    (this is when the device grabs the data bit).
			case WriteState::WaitClockHigh:
				if ( fastDigitalRead( PIN_CLK ) == HIGH )
				{
					m_write_state = WriteState::WaitClockLow;
					setExternalTimeout( 1500 );
				}
				break;
			// 10) Wait for the device to bring data low (for the ACK)
			case WriteState::WaitDataLow:
				if ( fastDigitalRead( PIN_DAT ) == LOW )
				{
					m_write_state = WriteState::WaitFinalClockLow;
					setExternalTimeout( 1500 );
				}
				break;
			// 11) Wait for the device to bring clock low
			case WriteState::WaitFinalClockLow:
				if ( fastDigitalRead( PIN_CLK ) == LOW )
				{
					m_write_state = WriteState::WaitForRelease;
					setExternalTimeout( 1500 );
				}
				break;
			case WriteState::WaitForRelease:
				if ( ( fastDigitalRead( PIN_CLK ) == HIGH ) &&
				     ( fastDigitalRead( PIN_DAT ) == HIGH ) )
				{
					// All done, so remove from buffer
					uint8_t b;
					m_to_kb_buffer.pop( b );
					reenable();
				}
				break;
		}
	}
#endif

	/// Handle an IRQ whilst we are reading data from the device.
	///
	/// This means another bit has been sent.
	void handleClockEdgeReadingWord( Edge edge, Level data_bit )
	{
		// Only care about falling edges when reading
		if ( edge == Edge::EDGE_FALLING )
		{
			if ( data_bit == Level::LEVEL_HIGH )
			{
				m_current_word |= m_current_word_bitmask;
			}

			m_current_word_bitmask <<= 1;
			if ( m_current_word_bitmask == PS2_INCOMING_MASK )
			{
				// Got all the bits - but are they good?
				m_valid_word = validateWord( m_current_word );
				m_state = State::Idle;
			}
			else
			{
				// Need more bits - set a timeout for the next bit
				setTimeout( 250 );
			}
		}
	}

	/**
	 * Handle poll when we are in the middle of reading data from the device.
	 */
	void handlePollReadingWord( void )
	{
		if ( hasTimedOut() )
		{
			// Hmm ... keyboard stopped part way through for 1..2ms?
			// Give up.
			m_state = State::Idle;
		}
	}

	/**
	 * After this many polls, we give up.
	 */
	void setTimeout( uint16_t timeout_count )
	{
		m_timeout_count = timeout_count;
		m_have_timeout = true;
	}

	/**
	 * Has the timeout expired?
	 */
	bool hasTimedOut( void )
	{
		bool result = false;
		if ( m_have_timeout )
		{
			result = ( m_timeout_count == 0 );
		}
		return result;
	}

	/**
	 * Convert an 8-bit byte into a 11-bit word suitable for clocking out to
	 * our remote device.
	 */
	static uint16_t encodeByte( uint8_t byte )
	{
		uint16_t result = 0;
		bool parity = true;
		for ( size_t i = 0; i < 8; i++ )
		{
			if ( bitRead( byte, i ) )
			{
				parity = !parity;
			}
		}
		// Data byte (first bit is start bit, which is zero)
		result |= ( (uint16_t)byte ) << 1;
		// Parity bit
		result |= ( parity ? 1 : 0 ) << 9;
		// Stop bit
		result |= 1 << 10;
		return result;
	}

	/**
	 * Check an 11-bit word we have received from the device.
	 *
	 * If it looks good, return the 8 data bits. If not, return -1.
	 */
	static int validateWord( uint16_t ps2_bits )
	{
		bool parity_bit = bitRead( ps2_bits, PARITY_BIT ) != 0;
		bool start_bit = bitRead( ps2_bits, START_BIT ) != 0;
		bool stop_bit = bitRead( ps2_bits, STOP_BIT ) != 0;
		bool parity = parity_bit;
		for ( uint8_t i = FIRST_DATA_BIT; i <= LAST_DATA_BIT; i++ )
		{
			if ( bitRead( ps2_bits, i ) )
			{
				parity = !parity;
			}
		}
		if ( !start_bit && parity && stop_bit )
		{
			return ( ps2_bits >> 1 ) & 0x00FF;
		}
		else
		{
			return -1;
		}
	}
};
