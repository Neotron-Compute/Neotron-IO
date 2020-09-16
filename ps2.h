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
 * This project is also licensed under the GPL (v3 or later version, at your
 * choice). See the [LICENCE](./LICENCE) file.
 */

#include <assert.h>
#include <string.h>

#include "DigitalPin.h"
#include "RingBuf.h"

extern void irqDebugValue( uint8_t x );
#define ps2DebugValue(x) irqDebugValue(x)
// #define ps2DebugValue(x)

#define fastInputPullup(pin) fastPinConfig(pin, false, true)
#define fastOutputLow(pin) fastPinConfig(pin, true, false)

enum class Ps2State
{
	Idle,
	ReadingWord,
	WritingWord,
	BufferFull,
	Disabled
};

enum class Ps2WriteState
{
	HoldingClock,
	WaitClockLow,
	WaitClockHigh,
	WaitDataLow,
	WaitFinalClockLow,
	WaitForRelease
};

/**
 * Represents a generic PS2 device.
 */
template <int PIN_CLK, int PIN_DAT>
class Ps2
{
   public:
	/**
	 * Construct a new Ps2 object.
	 *
	 * We do not attach an interrupt - you need to do that yourself (e.g. in a
	 * pin-change interrupt).
	 *
	 * @param pin_clk the Arduino pin number for the PS/2 clock pin
	 * @param pin_data the Arduino pin number for the PS/2 data pin
	 */
	Ps2()
	    : m_state( Ps2State::Idle ),
	      m_write_state( Ps2WriteState::HoldingClock ),
	      m_last_clk( true ),
	      m_internal_timeout( false ),
	      m_in_buffer(),
	      m_out_buffer(),
	      m_current_word( 0 ),
	      m_current_word_bitmask( 1 ),
	      m_timeout( 0 )
	{
		// Use internal pull-ups
		fastInputPullup( PIN_CLK );
		fastInputPullup( PIN_DAT );
	}

	/**
	 * Are we currently talking to the PS/2 device (either reading or writing a
	 * word)?
	 */
	bool isActive()
	{
		return ( m_state == Ps2State::ReadingWord ) ||
		       ( m_state == Ps2State::WritingWord );
	}

	/**
	 * Call this on a falling edge of the clock signal
	 */
	void irq()
	{
		if ( isEdge() )
		{		
			switch ( m_state )
			{
				case Ps2State::Idle:
					ps2DebugValue( 1 );
					irqIdle();
					break;
				case Ps2State::BufferFull:
				case Ps2State::Disabled:
					ps2DebugValue( 2 );
					// Should never happen!
					break;
				case Ps2State::WritingWord:
					ps2DebugValue( 3 );
					irqWritingWord();
					break;
				case Ps2State::ReadingWord:
					irqReadingWord();
					break;
			}		
		}
	}

	/**
	 * Checks the clock pin and reads a bit as required.
	 *
	 * You must call this often to avoid missing clock pulse edges.
	 */
	void poll()
	{
		switch ( m_state )
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
	 *
	 * I don't suggest you do this while clocking out a byte, unless you want
	 * to abort. Make sure you're in Idle first.
	 */
	void disable()
	{
		noInterrupts();
		fastOutputLow( PIN_CLK );
		m_state = Ps2State::Disabled;
		m_current_word = 0;
		m_current_word_bitmask = 1;
		interrupts();
	}

	/**
	 * Re-enables the PS/2 port by releasing the clock line.
	 *
	 * Also resets our system state.
	 */
	void renable()
	{
		noInterrupts();
		fastInputPullup( PIN_CLK );
		fastInputPullup( PIN_DAT );
		m_state = Ps2State::Idle;
		m_current_word = 0;
		m_current_word_bitmask = 1;
		interrupts();
	}

	/**
	 * Write data to the PS/2 device.
	 *
	 * The data is buffered and will be clocked out in subsequent calls to
	 * `poll`.
	 *
	 * @param data the bytes to write to the device
	 * @param data_len the number of bytes pointed to by `data`
	 *
	 * @return true if space in internal buffer to accept all the bytes, false
	 * if not enough space and write is rejected.
	 */
	bool writeBuffer( const uint8_t* data, size_t data_len )
	{
		if ( ( data_len + m_out_buffer.size() ) > m_out_buffer.maxSize() )
		{
			return false;
		}
		for ( size_t i = 0; i < data_len; i++ )
		{
			m_out_buffer.lockedPush( data[i] );
		}
	}

	/**
	 * Get a byte from the keyboard buffer. Returns -1 if the buffer is empty.
	 */
	int readBuffer()
	{
		if ( m_in_buffer.isEmpty() )
		{
			return -1;
		}
		else
		{
			uint8_t result;
			m_in_buffer.lockedPop( result );
			if ( m_state == Ps2State::BufferFull )
			{
				renable();
			}
			return result;
		}
	}

	/// Debug function - gets internal state
	Ps2State getState() {
		return m_state;
	}

	/// Debug function - gets internal state
	Ps2WriteState getWriteState() {
		return m_write_state;
	}

#ifndef TEST_MODE_NO_PRIVATE
   private:
#endif

	/// Handle a poll when we are in the idle state.
	///
	/// Check if we want to start a transmission.
	void pollIdle()
	{
		if ( !m_out_buffer.isEmpty() )
		{
			// We are idle and we have words waiting to clock out
			m_state = Ps2State::WritingWord;
			m_write_state = Ps2WriteState::HoldingClock;
			uint8_t b;
			// We don't pop this until it has been sent
			m_out_buffer.peek( b );
			m_current_word = encodeByte( b );
			// Skip the start bit
			m_current_word_bitmask = 2;
			// 1) Bring the Clock line low for at least 100 microseconds
			fastOutputLow( PIN_CLK );
			setInternalTimeout( 150 );
		}
	}

	/// Handle an IRQ when we are in the idle state.
	///
	/// This means we have a new byte coming in.
	void irqIdle()
	{
		if ( !m_last_clk )
		{
			// We have a falling edge
			m_current_word_bitmask = 1;
			m_state = Ps2State::ReadingWord;			
		}
	}

	bool isEdge() 
	{
		bool kb_clk_pin = fastDigitalRead( PIN_CLK );
		bool result = false;
		if ( kb_clk_pin != m_last_clk )
		{
			result = true;
		}
		m_last_clk = kb_clk_pin;
		return result;
	}

	/// Handle a poll whilst we are writing out data to the device.
	void pollWritingWord()
	{
		if ( !m_internal_timeout && hasTimedOut() )
		{
			// Hmm ... keyboard stopped part way through for 1..2ms?
			// Give up.
			renable();
		}
	}

	/**
	 * Handle a clock edge whilst writing out data.
	 */
	void irqWritingWord()
	{
		switch ( m_write_state )
		{
			case Ps2WriteState::HoldingClock:
				if ( hasTimedOut() )
				{
					ps2DebugValue( 5 );
					m_write_state = Ps2WriteState::WaitClockLow;
					// 2) Bring the Data line low.
					fastOutputLow( PIN_DAT );
					// 3) Release the Clock line.
					fastInputPullup( PIN_CLK );
				}
				break;
			// 4) Wait for the device to bring the clock line low
			//    (this is when we set the data line).
			case Ps2WriteState::WaitClockLow:
				if ( fastDigitalRead( PIN_CLK ) == LOW )
				{
					ps2DebugValue( 6 );
					// Are we done?
					if ( m_current_word_bitmask == PS2_OUTGOING_MASK )
					{
						// 9) All data + parity clocked out - time to Release
						// data line
						fastInputPullup( PIN_DAT );
						m_write_state = Ps2WriteState::WaitDataLow;
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
						m_write_state = Ps2WriteState::WaitClockHigh;
						setExternalTimeout( 1500 );
					}
				}
				break;
			// 6) Wait for device to bring the clock line high
			//    (this is when the device grabs the data bit).
			case Ps2WriteState::WaitClockHigh:
				if ( fastDigitalRead( PIN_CLK ) == HIGH )
				{
					ps2DebugValue( 7 );
					m_write_state = Ps2WriteState::WaitClockLow;
					setExternalTimeout( 1500 );
				}
				break;
			// 10) Wait for the device to bring data low (for the ACK)
			case Ps2WriteState::WaitDataLow:
				if ( fastDigitalRead( PIN_DAT ) == LOW )
				{
					ps2DebugValue( 8 );
					m_write_state = Ps2WriteState::WaitFinalClockLow;
					setExternalTimeout( 1500 );
				}
				break;
			// 11) Wait for the device to bring clock low
			case Ps2WriteState::WaitFinalClockLow:
				if ( fastDigitalRead( PIN_CLK ) == LOW )
				{
					ps2DebugValue( 9 );
					m_write_state = Ps2WriteState::WaitForRelease;
					setExternalTimeout( 1500 );
				}
				break;
			case Ps2WriteState::WaitForRelease:
				if ( ( fastDigitalRead( PIN_CLK ) == HIGH ) &&
				     ( fastDigitalRead( PIN_DAT ) == HIGH ) )
				{
					ps2DebugValue( 10 );
					// All done, so remove from buffer
					uint8_t b;
					m_out_buffer.pop( b );
					renable();
				}
				break;
		}
	}

	/**
	 * Handle poll when we are in the middle of reading data from the device.
	 */
	void pollReadingWord()
	{
		// if ( !m_internal_timeout && hasTimedOut() )
		// {
		// 	ps2DebugValue( 11 );
		// 	// Hmm ... keyboard stopped part way through for 1..2ms?
		// 	// Give up.
		// 	m_state = Ps2State::Idle;
		// 	m_current_word_bitmask = 1;
		// 	m_current_word = 0;
		// }
	}

	/// Handle an IRQ whilst we are reading data from the device.
	///
	/// This means another bit has been sent.
	void irqReadingWord()
	{
		// Falling Edge?
		if ( !m_last_clk )
		{
			if ( fastDigitalRead( PIN_DAT ) )
			{
				m_current_word |= m_current_word_bitmask;
			}
			m_current_word_bitmask <<= 1;
			if ( m_current_word_bitmask == PS2_INCOMING_MASK )
			{
				ps2DebugValue( m_current_word >> 1 );
				int result = validateWord( m_current_word );
				if ( result >= 0 )
				{
					ps2DebugValue( 0xD0 );
					m_in_buffer.push( result );
					if ( m_in_buffer.isFull() )
					{
						fastOutputLow( PIN_CLK );
						m_state = Ps2State::BufferFull;
						m_current_word = 0;
						m_current_word_bitmask = 1;
					}
					m_current_word_bitmask = 1;
				} else {
					ps2DebugValue( 0xD1 );
				}
			}
			setExternalTimeout( 2500 );
		}
	}

	/**
	 * Waits for `num_micros` microseconds before we do something else.
	 */
	void setInternalTimeout( uint16_t num_micros )
	{
		assert( num_micros < 32000 );
		m_timeout = ( ( uint16_t )( micros() & 0xFFFF ) ) + num_micros;
		m_internal_timeout = true;
	}

	/**
	 * Waits for `num_micros` microseconds for the external device to do something.
	 */
	void setExternalTimeout( uint16_t num_micros )
	{
		setInternalTimeout( num_micros );
		m_internal_timeout = false;
	}

	/**
	 * Has the timeout expired?
	 */
	bool hasTimedOut()
	{
		uint16_t now = micros() & 0xFFFF;
		int16_t delta = m_timeout - now;
		if ( delta <= 0 )
		{
			m_timeout = 0;
			return true;
		}
		else
		{
			return false;
		}
	}

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
	 * Check an 11-bit word from the device.
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

	static constexpr uint8_t PARITY_BIT = 9;
	static constexpr uint8_t STOP_BIT = 10;
	static constexpr uint8_t START_BIT = 0;
	static constexpr uint8_t FIRST_DATA_BIT = 1;
	static constexpr uint8_t LAST_DATA_BIT = 8;
	static constexpr size_t IN_BUFFER_SIZE = 32;
	static constexpr size_t OUT_BUFFER_SIZE = 32;
	static constexpr size_t PS2_INCOMING_MASK = 1 << 11;
	static constexpr size_t PS2_OUTGOING_MASK = 1 << 10;
	static constexpr uint16_t TIMEOUT_POLLS = 800;

	volatile Ps2State m_state;
	volatile Ps2WriteState m_write_state;
	volatile bool m_internal_timeout;
	bool m_last_clk;
	uint16_t m_current_word;
	size_t m_current_word_bitmask;
	RingBuf<uint8_t, IN_BUFFER_SIZE> m_in_buffer;
	RingBuf<uint8_t, OUT_BUFFER_SIZE> m_out_buffer;
	uint16_t m_timeout;
};
