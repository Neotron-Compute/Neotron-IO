/**
   This is the Neotron IO controller firmware. It is basically
   a two channel PS/2 to single channel UART adaptor.

    - Copyright: (c) Jonathan 'theJPster' Pallant, 2020
    - Licence: Licensed under the GPL (v3 or later version, at your choice). See the [LICENCE](./LICENCE) file.
    - Version: 0.9.0
*/

/// Firmware version
#define VERSION_STRING "v0.9.0"

/// How long we wait for a command over the UART
#define CMD_TIMEOUT_MS 1000

/// The size of the data buffers. Should be a power of 2 for efficiency.
#define BUFFER_LEN 16

/// Maximum number of ms before we reset the bit collector.
#define MAX_DELAY 100

/// Keyboard PS/2 Clock, on INT0. Must be on an interrupt pin.
static constexpr uint8_t KB_CLK = 2;

/// Keyboard PS/2 Data. Can be on any pin.
static constexpr uint8_t KB_DAT = 4;

/// Mouse PS/2 Clock, on INT1. Must be on an interrupt pin.
static constexpr uint8_t MS_CLK = 3;

/// Mouse PS/2 Data. Can be on any pin.
static constexpr uint8_t MS_DAT = 5;

template <uint8_t DAT_PIN>
class Ps2
{
   public:
	/// Returned from read when buffer empty:
	static constexpr uint16_t ERR_EMPTY = 0x100;

	/// Returned from read when parity error:
	static constexpr uint16_t ERR_PARITY = 0x200;

	/// Got into a bad state
	static constexpr uint16_t ERR_STATE = 0x300;

	/**
	 * Constructor
	 */
	Ps2()
	    : bit_count( 0 ),
	      odd_parity( false ),
	      data( 0 ),
	      last_time( 0 ),
	      widx( 0 ),
	      ridx( 0 )
	{
	}

	/**
	   Resets the bit count
	*/
	void reset( void )
	{
		bit_count = 0;
		odd_parity = false;
		data = 0;
	}

	/**
	   Call this on a falling clock edge.
	*/
	void irq( void )
	{
		unsigned long now = millis();
		if ( ( now - last_time ) > MAX_DELAY )
		{
			reset();
		}

		switch ( bit_count++ )
		{
			case 0:
				// Start bit - ignore
				break;
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
				// Data bits
				data >>= 1;
				if ( digitalRead( DAT_PIN ) != LOW )
				{
					data |= 0x80;
					odd_parity ^= true;
				}
				break;
			case 9:
				// Parity bit
				if ( digitalRead( DAT_PIN ) != LOW )
				{
					odd_parity ^= true;
				}
				break;
			case 10:
				// Stop bit - time to check we have odd parity
				if ( odd_parity )
				{
					// Store good data
					buffer[widx % BUFFER_LEN] = data;
				}
				else
				{
					// Store obviously bad data
					buffer[widx % BUFFER_LEN] = ERR_PARITY | data;
				}
				widx += 1;
				reset();
				break;
			default:
				// Should never get here...
				buffer[widx % BUFFER_LEN] = ERR_STATE;
				widx += 1;
				break;
		}
		last_time = now;
	}

	/**
	   Get the next byte from the received bytes buffer.
	*/
	uint16_t read( void )
	{
		if ( ( ridx != widx ) )
		{
			uint16_t result = buffer[ridx % BUFFER_LEN];
			ridx += 1;
			return result;
		}
		else
		{
			return ERR_EMPTY;
		}
	}

   private:
	/// This is where we track how many bits we have received.
	uint8_t bit_count;
	/// This is where we track the number of `1` bits in the received data.
	bool odd_parity;
	/// This is where we collect the bits from the device.
	uint8_t data;
	/// This is the last time we received an IRQ, in milliseconds since startup
	/// (see `millis()`).
	unsigned long last_time;
	/// This is where we collect bytes received from the PS/2 device ready to
	/// send to the host.
	volatile uint16_t buffer[BUFFER_LEN];
	/// The `buffer` write index. Wraps at 255, so use `% BUFFER_LEN` when
	/// accessing buffer.
	volatile uint8_t widx;
	/// The `buffer` read index. Wraps at 255, so use `% BUFFER_LEN` when
	/// accessing buffer.
	volatile uint8_t ridx;
};

Ps2<KB_DAT> keyboard;
Ps2<MS_DAT> mouse;

/**
   Called when keyboard clock has falling edge.
*/
void kb_irq( void )
{
	keyboard.irq();
}

/**
   Called when mouse clock has falling edge.
*/
void ms_irq( void )
{
	mouse.irq();
}

/**
   Stop the two PS/2 receivers.

   Any packets that are mid-transmission will get re-sent by the remote device.
*/
void stop_rx()
{
	// Data = high, Clock = low: Communication Inhibited.
	pinMode( KB_DAT, INPUT_PULLUP );
	pinMode( MS_DAT, INPUT_PULLUP );
	detachInterrupt( digitalPinToInterrupt( KB_CLK ) );
	detachInterrupt( digitalPinToInterrupt( MS_CLK ) );
	digitalWrite( KB_CLK, LOW );
	pinMode( KB_CLK, OUTPUT );
	digitalWrite( MS_CLK, LOW );
	pinMode( MS_CLK, OUTPUT );
	keyboard.reset();
	mouse.reset();
}

/**
   Start the two PS/2 receivers.
*/
void start_rx()
{
	// Data = high, Clock = high: Idle state.
	keyboard.reset();
	mouse.reset();
	attachInterrupt( digitalPinToInterrupt( KB_CLK ), kb_irq, FALLING );
	attachInterrupt( digitalPinToInterrupt( MS_CLK ), ms_irq, FALLING );
	pinMode( KB_DAT, INPUT_PULLUP );
	pinMode( MS_DAT, INPUT_PULLUP );
	pinMode( KB_CLK, INPUT_PULLUP );
	pinMode( KB_CLK, INPUT_PULLUP );
}

/**
 * Busy-wait (up to a certain number of milliseconds) for a serial byte to
 * arrive.
 */
int read_with_timeout( uint32_t timeout_ms )
{
	uint32_t start = millis();
	while ( ( millis() - start ) < timeout_ms )
	{
		if ( Serial.available() )
		{
			return Serial.read();
		}
	}
	return -1;
}

/**
 * Busy-wait (up to a certain number of milliseconds) for a pin to reach a
 * level.
 */
bool wait_for_level( int pin, int wanted_level, uint32_t timeout_ms )
{
	uint32_t start = millis();
	while ( ( millis() - start ) < timeout_ms )
	{
		int current_level = digitalRead( pin );
		if ( current_level == wanted_level )
		{
			return true;
		}
	}
	return false;
}

/*
   The host wants to send a byte to the given device.
*/
uint8_t handle_command( const uint8_t clk_pin, const uint8_t dat_pin )
{
	// Read hex byte
	int nibble1 = read_with_timeout( CMD_TIMEOUT_MS );
	if ( !isxdigit( nibble1 ) )
	{
		return 1;
	}

	int nibble2 = read_with_timeout( CMD_TIMEOUT_MS );
	if ( !isxdigit( nibble2 ) )
	{
		return 2;
	}

	int lf = read_with_timeout( CMD_TIMEOUT_MS );
	if ( lf != '\n' )
	{
		return 3;
	}

	uint8_t data = 0;

	// Parse top nibble
	if ( ( nibble1 >= 'a' ) && ( nibble1 <= 'f' ) )
	{
		data = ( 10 + nibble1 - 'a' ) << 4;
	}
	else if ( ( nibble1 >= 'A' ) && ( nibble1 <= 'F' ) )
	{
		data = ( 10 + nibble1 - 'A' ) << 4;
	}
	else
	{
		data = ( nibble1 - '0' ) << 4;
	}

	// Parse bottom nibble
	if ( ( nibble2 >= 'a' ) && ( nibble2 <= 'f' ) )
	{
		data |= ( 10 + nibble2 - 'a' );
	}
	else if ( ( nibble2 >= 'A' ) && ( nibble2 <= 'F' ) )
	{
		data |= ( 10 + nibble2 - 'A' );
	}
	else
	{
		data |= ( nibble2 - '0' );
	}

	// 1)   Bring the Clock line low for at least 100 microseconds to inhibit
	// comms. We did this before we entered this function and we've delayed a
	// load already waiting for the Serial bytes, but a bit more won't hurt.

	delayMicroseconds( 100 );

	// Data = low, Clock = high: Host Request-to-Send
	// Note that data sent from the host to the device is read on the rising
	// edge

	// 2)   Bring the Data line low.
	digitalWrite( dat_pin, LOW );
	pinMode( dat_pin, OUTPUT );

	// 3)   Release the Clock line.
	pinMode( clk_pin, INPUT_PULLUP );

	// 4)   Wait for the device to bring the Clock line low.
	// 15ms is the upper limit specified in the protocol
	if ( !wait_for_level( clk_pin, LOW, 15 ) )
	{
		return 4;
	}

	bool odd_parity = false;
	for ( uint8_t bits = 0; bits < 8; bits++ )
	{
		// 5)   Set/reset the Data line to send the first data bit
		if ( data & 1 )
		{
			digitalWrite( dat_pin, HIGH );
			odd_parity ^= true;
		}
		else
		{
			digitalWrite( dat_pin, LOW );
		}
		data >>= 1;
		// 6)   Wait for the device to bring Clock high.
		// We should only wait 2ms for the whole byte, but this will do.
		if ( !wait_for_level( clk_pin, HIGH, 3 ) )
		{
			return 5;
		}
		// 7)   Wait for the device to bring Clock low.
		if ( !wait_for_level( clk_pin, LOW, 3 ) )
		{
			return 6;
		}
		// 8)   Repeat steps 5-7 for the *other seven data bits* and the parity
		// bit
	}

	// 8)   Repeat steps 5-7 for the parity bit
	digitalWrite( dat_pin, odd_parity ? LOW : HIGH );
	if ( !wait_for_level( clk_pin, HIGH, 3 ) )
	{
		return 7;
	}
	if ( !wait_for_level( clk_pin, LOW, 3 ) )
	{
		return 8;
	}

	// 9)   Send a stop bit
	digitalWrite( dat_pin, HIGH );
	if ( !wait_for_level( clk_pin, HIGH, 3 ) )
	{
		return 9;
	}
	if ( !wait_for_level( clk_pin, LOW, 3 ) )
	{
		return 10;
	}

	// 10)   Release the Data line.
	pinMode( dat_pin, INPUT_PULLUP );

	// 11) Wait for the device to bring Clock low.
	if ( !wait_for_level( clk_pin, LOW, 50 ) )
	{
		return 11;
	}

	// 12) Read the ACK bit on the falling edge of the clock
	bool ack_bit = digitalRead( dat_pin );

	// 13) Wait for the device to release Clock
	if ( !wait_for_level( clk_pin, HIGH, 50 ) )
	{
		return 12;
	}

	// Inhibit comms again
	digitalWrite( clk_pin, LOW );
	pinMode( clk_pin, OUTPUT );

	return ack_bit ? 13 : 0;
}

/**
   Set-up routine. Called once.
*/
void setup()
{
	Serial.begin( 9600 );
	Serial.println( "V: Neotron IO " VERSION_STRING );
	start_rx();
}

/**
   Main routine. Called repeatedly.
*/
void loop()
{
	static const char HEX_CHARS[] = "0123456789ABCDEF";

	if ( Serial.availableForWrite() )
	{
		int data = keyboard.read();
		if ( data != keyboard.ERR_EMPTY )
		{
			if ( data >= 0x100 )
			{
				Serial.write( 'E' );
				Serial.write( HEX_CHARS[( data >> 12 ) & 0x0F] );
				Serial.write( HEX_CHARS[( data >> 8 ) & 0x0F] );
			}
			else
			{
				Serial.write( 'K' );
			}
			Serial.write( HEX_CHARS[( data >> 4 ) & 0x0F] );
			Serial.write( HEX_CHARS[data & 0x0F] );
			Serial.write( '\n' );
		}
	}

	if ( Serial.availableForWrite() )
	{
		int data = mouse.read();
		if ( data != mouse.ERR_EMPTY )
		{
			if ( data >= 0x100 )
			{
				Serial.write( 'O' );
				Serial.write( HEX_CHARS[( data >> 12 ) & 0x0F] );
				Serial.write( HEX_CHARS[( data >> 8 ) & 0x0F] );
			}
			else
			{
				Serial.write( 'M' );
			}
			Serial.write( HEX_CHARS[( data >> 4 ) & 0x0F] );
			Serial.write( HEX_CHARS[data & 0x0F] );
			Serial.write( '\n' );
		}
	}

	if ( Serial.available() )
	{
		uint8_t e = 0;
		char cmd = Serial.read();
		switch ( cmd )
		{
			case 'K':
				// Disable all devices by holding clock low
				stop_rx();
				// Process keyboard command
				e = handle_command( KB_CLK, KB_DAT );
				// Start receivers again
				start_rx();
				if ( e != 0 )
				{
					Serial.write( 'S' );
					Serial.write( HEX_CHARS[( e >> 4 ) & 0x0F] );
					Serial.write( HEX_CHARS[e & 0x0F] );
					Serial.write( '\n' );
				}
				else
				{
					Serial.println( "OK" );
				}
				break;
			case 'M':
				// Disable all devices by holding clock low
				stop_rx();
				// Process mouse command
				e = handle_command( MS_CLK, MS_DAT );
				// Start receivers again
				start_rx();
				if ( e != 0 )
				{
					Serial.write( 'S' );
					Serial.write( HEX_CHARS[( e >> 4 ) & 0x0F] );
					Serial.write( HEX_CHARS[e & 0x0F] );
					Serial.write( '\n' );
				}
				else
				{
					Serial.println( "OK" );
				}
				break;
			default:
				break;
		}
	}
}

// End of file