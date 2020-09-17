/**
   Neotron-IO Firmware.

   Uses MiniCore for the AtMega328P. Set to 8 MHz Internal RC. Don't go higher
   than 9600 baud.

   This project is also licensed under the GPL (v3 or later version, at your
   choice). See the [LICENCE](./LICENCE) file.
*/

#define REAL_ARDUINO_UNO

//
// Includes
//
#include <EEPROM.h>

#include "DigitalPin.h"
#include "RingBuf.h"
#include "joystick.h"
#include "ps2.h"

//
// Constants
//

// Pins D0 and D1 are for the UART
const int JS1_PIN_START_C = 2;
const int JS1_PIN_GND_RIGHT = 3;
const int JS1_PIN_GND_LEFT = 4;
const int JS1_PIN_DOWN = 5;
const int JS1_PIN_AB = 6;
const int JS1_PIN_UP = 7;

#ifdef REAL_ARDUINO_UNO
const int JS2_PIN_AB = 8;
const int JS2_PIN_UP = 9;
#elif NEOTRON_32
const int JS2_PIN_AB = 20;
const int JS2_PIN_UP = 21;
#else
#error Please select a supported board
#endif
const int JS2_PIN_START_C = 10;
const int JS2_PIN_GND_RIGHT = 11;
const int JS2_PIN_GND_LEFT = 12;
const int JS2_PIN_DOWN = 13;

// NOTE: Do not change KB_CLK and MS_CLK pins without re-configuring Pin
// Change interrupts!
const int KB_CLK = A0;
const uint8_t KB_CLK_PIN_MASK = _BV( 0 );
const int KB_DAT = A1;
const uint8_t KB_DAT_PIN_MASK = _BV( 1 );
const int MS_CLK = A2;
const uint8_t MS_CLK_PIN_MASK = _BV( 2 );
const int MS_DAT = A3;
const uint8_t MS_DAT_PIN_MASK = _BV( 3 );

const int JS2_PIN_SELECT = A4;
const int JS1_PIN_SELECT = A5;

const uint8_t EEPROM_MAGIC_BYTE = 0xE0;
const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_OSCCAL = 1;

const int CALIBRATION_OUT = MS_DAT;

const uint16_t DEBOUNCE_LOOPS = 5;

const size_t MAX_INPUT_BUFFER = 16;

//
// Private Function Declarations
//

static void keyboardClockInput( void );
static void mouseClockInput( void );
static void keyboardClockOutput( Ps2::Level level );
static void mouseClockOutput( Ps2::Level level );
static void keyboardDataInput( void );
static void mouseDataInput( void );
static void keyboardDataOutput( Ps2::Level level );
static void mouseDataOutput( Ps2::Level level );
static void processInput( char inputChar );
static void kbClockPinChangeIrq( void );
static void mouseClockPinChangeIrq( void );
static void bufferPrint( const String& s );
static void bufferPrintln( void );
static void bufferPrintln( const String& s );
static void bufferPrintHex( uint16_t value );
static void bufferPrintHex2( uint8_t value );
static char wordToHex( uint16_t value, uint8_t nibble_idx );

//
// Variables
//

static Joystick<JS1_PIN_UP,
                JS1_PIN_DOWN,
                JS1_PIN_GND_LEFT,
                JS1_PIN_GND_RIGHT,
                JS1_PIN_AB,
                JS1_PIN_START_C,
                JS1_PIN_SELECT>
    gJs1;
static Joystick<JS2_PIN_UP,
                JS2_PIN_DOWN,
                JS2_PIN_GND_LEFT,
                JS2_PIN_GND_RIGHT,
                JS2_PIN_AB,
                JS2_PIN_START_C,
                JS2_PIN_SELECT>
    gJs2;
static RingBuf<char, 256> gSerialBuffer;
static bool gCalibrationMode = 0;
static Ps2 gKeyboard( keyboardClockInput,
                      keyboardClockOutput,
                      keyboardDataInput,
                      keyboardDataOutput );
static Ps2 gMouse( mouseClockInput,
                   mouseClockOutput,
                   mouseDataInput,
                   mouseDataOutput );

//
// Functions
//

/**
 * The setup function runs once when you press reset or power the board.
 */
void setup( void )
{
	if ( EEPROM.read( EEPROM_ADDR_MAGIC ) == EEPROM_MAGIC_BYTE )
	{
		// Load OSCCAL
		OSCCAL = EEPROM.read( EEPROM_ADDR_OSCCAL );
	}

	// One of the few 'standard' baud rates you can easily hit from an 8 MHz
	// clock
	Serial.begin( 9600 );
	// Sign-on banner
	Serial.print( "b020\n" );

	if ( gJs1.scan() )
	{
		JoystickResult test_for_cal_mode = gJs1.read();
		// Press up and down simultaneously on start-up to enter cal-mode
		gCalibrationMode = test_for_cal_mode.is_up_pressed() &&
		                   test_for_cal_mode.is_down_pressed();
	}
	if ( gCalibrationMode )
	{
		// The frequency of this output should be 8 MHz / (256 * 64 * 2), or
		// 244.14 Hz. You need to adjust OSCCAL until you get within 5% for a
		// functioning UART - the closer the better as it drifts over
		// temperature.
		analogWrite( CALIBRATION_OUT, 128 );
	}

	// OK, so the PS/2 input clock is up to 16.7 kHz
	// That gives us 60 microseconds per cycle (e.g. falling to falling)
	// Or 30 microseconds between each clock edge (e.g. rising to falling)
	// At 8 MHz, we have 8 instructions per microsecond
	// So we only have 240 microseconds to spot the edge and do something about
	// it!

	// When reading from a PS/2 device (the default), we just need to sample the
	// data pin at the falling clock edge and shift it into a register.

	// Our clock and data pins are on PORTC. So we can set up a Port C
	// pin-change interrupt. Or we can move the PS/2 clock lines to D2 and D3,
	// which support individual external interrupts.

	// Set all as inputs with pull-ups
	keyboardDataInput();
	keyboardClockInput();
	mouseDataInput();
	mouseClockInput();

	// Configure pin-change interrupt on port C - this is PCINT10 and PCINT18.
	PCMSK1 = KB_CLK_PIN_MASK | MS_CLK_PIN_MASK;
}

ISR( PCINT1_vect )
{
	static uint8_t old_pin_state = 0;
	uint8_t pin_state = PINC;
	uint8_t changed_pins = pin_state ^ old_pin_state;

	if ( ( changed_pins & KB_CLK_PIN_MASK ) )
	{
		bool keyboard_data_bit = ( pin_state & KB_DAT_PIN_MASK ) != 0;
		if ( pin_state & KB_CLK_PIN_MASK )
		{
			// Rising Keyboard Clock signal
			gKeyboard.clockEdge( Ps2::Edge::EDGE_RISING,
			                     keyboard_data_bit ? Ps2::Level::LEVEL_HIGH
			                                       : Ps2::Level::LEVEL_LOW );
		}
		else
		{
			// Falling Keyboard Clock signal
			gKeyboard.clockEdge( Ps2::Edge::EDGE_FALLING,
			                     keyboard_data_bit ? Ps2::Level::LEVEL_HIGH
			                                       : Ps2::Level::LEVEL_LOW );
		}
	}

	if ( ( changed_pins & MS_CLK_PIN_MASK ) )
	{
		bool mouse_data_bit = ( pin_state & MS_DAT_PIN_MASK ) != 0;
		if ( pin_state & MS_CLK_PIN_MASK )
		{
			// Rising Mouse Clock signal
			gMouse.clockEdge( Ps2::Edge::EDGE_RISING,
			                  mouse_data_bit ? Ps2::Level::LEVEL_HIGH
			                                 : Ps2::Level::LEVEL_LOW );
		}
		else
		{
			// Falling Mouse Clock signal
			gMouse.clockEdge( Ps2::Edge::EDGE_FALLING,
			                  mouse_data_bit ? Ps2::Level::LEVEL_HIGH
			                                 : Ps2::Level::LEVEL_LOW );
		}
	}

	old_pin_state = pin_state;
}

/**
 * The loop function runs over and over again forever.
 */
void loop( void )
{
	static uint16_t debounce_count = 0;
	JoystickResult js1_bits;
	JoystickResult js2_bits;

	// Process incoming characters from the host
	if ( Serial.available() )
	{
		char inputChar = Serial.read();
		processInput( inputChar );
	}

	// Process outbound characters for the host
	while ( Serial.availableForWrite() )
	{
		char data;
		if ( gSerialBuffer.lockedPop( data ) )
		{
			Serial.write( data );
		}
		else
		{
			break;
		}
	}

	// Process the keyboard
	int keyboardByte = gKeyboard.poll();
	if ( keyboardByte >= 0 )
	{
		bufferPrint( "k" );
		bufferPrintHex2( keyboardByte );
		bufferPrintln();
	}

	// Process the mouse
	int mouseByte = gMouse.poll();
	if ( mouseByte >= 0 )
	{
		bufferPrint( "m" );
		bufferPrintHex2( mouseByte );
		bufferPrintln();
	}

	// Process Joystick 1
	if ( gJs1.scan() )
	{
		js1_bits = gJs1.read();
		bufferPrint( "s" );
		bufferPrintHex( js1_bits.value() );
		bufferPrintln();
	}

	// Process Joystick 2
	if ( gJs2.scan() )
	{
		js2_bits = gJs2.read();
		bufferPrint( "t" );
		bufferPrintHex( js2_bits.value() );
		bufferPrintln();
	}

	if ( gCalibrationMode )
	{
		// Look at joystick and trim OSCCAL up or down
		if ( js1_bits.is_a_pressed() )
		{
			// Has the A button been down for enough time?
			if ( debounce_count == DEBOUNCE_LOOPS )
			{
				// Yes it has. If the are pressing up, increase OSCCAL.
				// If they are pressing down, decrease OSCCAL.
				if ( js1_bits.is_up_pressed() )
				{
					OSCCAL++;
				}
				else if ( js1_bits.is_down_pressed() )
				{
					OSCCAL--;
				}
				debounce_count++;
			}
			else if ( debounce_count < DEBOUNCE_LOOPS )
			{
				debounce_count++;
			}
		}
		else
		{
			debounce_count = 0;
		}
		if ( js1_bits.is_start_pressed() )
		{
			EEPROM.write( EEPROM_ADDR_MAGIC, EEPROM_MAGIC_BYTE );
			EEPROM.write( EEPROM_ADDR_OSCCAL, OSCCAL );
			while ( 1 )
			{
				// Lock up once we've saved the EEPROM
				bufferPrintln( "RESET ME" );
			}
		}
		// Whatever happens, print out the current OSCCAL
		bufferPrint( "O" );
		bufferPrintHex( OSCCAL );
		bufferPrintln();
	}
}

// =================================================
// Private Functions
// =================================================

static void keyboardClockInput( void )
{
	fastPinConfig( KB_CLK, false, true );
}

static void mouseClockInput( void )
{
	fastPinConfig( MS_CLK, false, true );
}

static void keyboardClockOutput( Ps2::Level level )
{
	fastPinConfig( KB_CLK, true, level == Ps2::Level::LEVEL_HIGH );
}

static void mouseClockOutput( Ps2::Level level )
{
	fastPinConfig( MS_CLK, true, level == Ps2::Level::LEVEL_HIGH );
}

static void keyboardDataInput( void )
{
	fastPinConfig( KB_DAT, false, true );
}

static void mouseDataInput( void )
{
	fastPinConfig( MS_DAT, false, true );
}

static void keyboardDataOutput( Ps2::Level level )
{
	fastPinConfig( KB_DAT, true, level == Ps2::Level::LEVEL_HIGH );
}

static void mouseDataOutput( Ps2::Level level )
{
	fastPinConfig( MS_DAT, true, level == Ps2::Level::LEVEL_HIGH );
}

/**
 * Handle a character received from the host PC.
 *
 * Generally we just do some hex decoding and drop it into the relevant
 * transmit buffer.
 */
static void processInput( char inputChar )
{
	enum class InputState
	{
		WantCommand,
		WantHiNibble,
		WantLoNibble,
		WantNewline,
	};

	static InputState inputState = InputState::WantCommand;
	static char inputTarget;
	static uint8_t inputByte;

	switch ( inputState )
	{
		case InputState::WantCommand:
			if ( ( inputChar == 'K' ) || ( inputChar == 'M' ) )
			{
				inputTarget = inputChar;
				inputState = InputState::WantHiNibble;
			}
			else if ( inputChar == 'I' )
			{
				bufferPrint( "Info: " );
				bufferPrintHex( gKeyboard.getState() );
				bufferPrint( " " );
				bufferPrintHex( gMouse.getState() );
				bufferPrintln( "." );
			}
			break;
		case InputState::WantHiNibble:
			if ( ( inputChar >= '0' ) && ( inputChar <= '9' ) )
			{
				inputByte = ( inputChar - '0' ) << 4;
				inputState = InputState::WantLoNibble;
			}
			else if ( ( inputChar >= 'A' ) && ( inputChar <= 'F' ) )
			{
				inputByte = ( 10 + inputChar - 'A' ) << 4;
				inputState = InputState::WantLoNibble;
			}
			else if ( ( inputChar >= 'a' ) && ( inputChar <= 'f' ) )
			{
				inputByte = ( 10 + inputChar - 'a' ) << 4;
				inputState = InputState::WantLoNibble;
			}
			else
			{
				reportError();
				inputState = InputState::WantCommand;
			}
			break;
		case InputState::WantLoNibble:
			if ( ( inputChar >= '0' ) && ( inputChar <= '9' ) )
			{
				inputByte |= ( inputChar - '0' );
				inputState = InputState::WantNewline;
			}
			else if ( ( inputChar >= 'A' ) && ( inputChar <= 'F' ) )
			{
				inputByte |= ( 10 + inputChar - 'A' );
				inputState = InputState::WantNewline;
			}
			else if ( ( inputChar >= 'a' ) && ( inputChar <= 'f' ) )
			{
				inputByte |= ( 10 + inputChar - 'a' );
				inputState = InputState::WantNewline;
			}
			else
			{
				reportError();
				inputState = InputState::WantCommand;
			}
			break;
		case InputState::WantNewline:
			if ( ( inputChar == '\r' ) || ( inputChar == '\n' ) )
			{
				if ( inputTarget == 'K' )
				{
					gMouse.disable();
					bufferPrint( "K" );
					bufferPrintHex2( inputByte );
					bufferPrintln();
					gKeyboard.sendByte( inputByte );
					gMouse.enable();
				}
				else if ( inputTarget == 'M' )
				{
					gKeyboard.disable();
					bufferPrint( "M" );
					bufferPrintHex2( inputByte );
					bufferPrintln();
					gMouse.sendByte( inputByte );
					gKeyboard.enable();
				}
			}
			else
			{
				reportError();
			}
			inputState = InputState::WantCommand;
			break;
	}
}

void irqDebugValue( uint8_t x )
{
	gSerialBuffer.push( wordToHex( x, 1 ) );
	gSerialBuffer.push( wordToHex( x, 0 ) );
	gSerialBuffer.push( '\n' );
}

/**
   Print to the serial buffer.
*/
static void bufferPrint( const String& s )
{
	for ( char const& c : s )
	{
		gSerialBuffer.lockedPush( c );
	}
}

/**
   Print a newline.
*/
static void bufferPrintln()
{
	bufferPrint( "\n" );
}

/**
   Print a string, then a newline.
*/
static void bufferPrintln( const String& s )
{
	bufferPrint( s );
	bufferPrint( "\n" );
}

/**
   Print a 16-bit word as four hex nibbles, big-endian.
*/
static void bufferPrintHex( uint16_t value )
{
	gSerialBuffer.lockedPush( wordToHex( value, 3 ) );
	gSerialBuffer.lockedPush( wordToHex( value, 2 ) );
	gSerialBuffer.lockedPush( wordToHex( value, 1 ) );
	gSerialBuffer.lockedPush( wordToHex( value, 0 ) );
}

/**
   Print an 8-bit byte as two hex nibbles, big-endian.
*/
static void bufferPrintHex2( uint8_t value )
{
	gSerialBuffer.lockedPush( wordToHex( value, 1 ) );
	gSerialBuffer.lockedPush( wordToHex( value, 0 ) );
}

/**
   Convert 4 bits from an integer to a single hex nibble.
*/
static char wordToHex( uint16_t value, uint8_t nibble_idx )
{
	static const char hexNibbles[] = { '0', '1', '2', '3', '4', '5', '6', '7',
	                                   '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	uint8_t nibble = ( value >> ( 4 * nibble_idx ) ) & 0x000F;
	return hexNibbles[nibble];
}

/**
   Report an error back to the host.
*/
static void reportError( void )
{
	bufferPrintln( "ERR" );
}

// End of file
