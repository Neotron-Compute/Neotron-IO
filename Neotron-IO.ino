/**
 * Neotron-IO Firmware.
 *
 * Uses MiniCore for the AtMega328P. Set to 8 MHz Internal RC. Don't go higher
 * than 9600 baud.
 *
 * This project is also licensed under the GPL (v3 or later version, at your
 * choice). See the [LICENCE](./LICENCE) file.
 */

#define REAL_ARDUINO_UNO

//
// Includes
//
#include <EEPROM.h>

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

const int KB_CLK = A0;
const int KB_DAT = A1;
const int MS_CLK = A2;
const int MS_DAT = A3;
const int JS2_PIN_SELECT = A4;
const int JS1_PIN_SELECT = A5;

const uint8_t EEPROM_MAGIC_BYTE = 0xE0;
const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_OSCCAL = 1;

const int CALIBRATION_OUT = MS_DAT;

const uint16_t DEBOUNCE_LOOPS = 5;

const size_t MAX_INPUT_BUFFER = 16;

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
static Ps2<KB_CLK, KB_DAT> gKeyboard;
static Ps2<MS_CLK, MS_DAT> gMouse;
static RingBuf<char, 256> gSerialBuffer;
static bool gCalibrationMode = 0;

//
// Private Function Declarations
//

static void bufferPrint( const String& s );
static void bufferPrintln();
static void bufferPrintln( const String& s );
static void bufferPrintHex( uint16_t value );
static void bufferPrintHex2( uint8_t value );
static char wordToHex( uint16_t value, uint8_t nibble_idx );

//
// Functions
//

// the setup function runs once when you press reset or power the board
void setup()
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
	Serial.print( "B020\n" );

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
}

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

static void processInput( char inputChar )
{
	switch ( inputState )
	{
		case InputState::WantCommand:
			if ( ( inputChar == 'K' ) || ( inputChar == 'M' ) )
			{
				inputTarget = inputChar;
				inputState = InputState::WantHiNibble;
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
				inputState = InputState::WantCommand;
			}
			break;
		case InputState::WantNewline:
			if ( ( inputChar == '\r' ) || ( inputChar == '\n' ) )
			{
				if ( inputTarget == 'K' )
				{
					gKeyboard.writeBuffer( &inputByte, 1 );
				}
				else if ( inputTarget == 'M' )
				{
					gKeyboard.writeBuffer( &inputByte, 1 );
				}
			}
			inputState = InputState::WantCommand;
			break;
	}
}

// the loop function runs over and over again forever
void loop()
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
	if ( Serial.availableForWrite() )
	{
		char data;
		if ( gSerialBuffer.pop( data ) )
		{
			Serial.write( data );
		}
	}

	// Process the keyboard
	gKeyboard.poll();
	int keyboardByte = gKeyboard.readBuffer();
	if ( keyboardByte >= 0 )
	{
		bufferPrint( "K" );
		bufferPrintHex2( keyboardByte );
		bufferPrintln();
	}

	// Process the mouse
	gMouse.poll();
	int mouseByte = gMouse.readBuffer();
	if ( mouseByte >= 0 )
	{
		bufferPrint( "M" );
		bufferPrintHex2( mouseByte );
		bufferPrintln();
	}

	// Process Joystick 1
	if ( gJs1.scan() )
	{
		js1_bits = gJs1.read();
		bufferPrint( "S" );
		bufferPrintHex( js1_bits.value() );
		bufferPrintln();
	}

	// Process Joystick 2
	if ( gJs2.scan() )
	{
		js2_bits = gJs2.read();
		bufferPrint( "T" );
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

/**
 * Print to the serial buffer.
 */
static void bufferPrint( const String& s )
{
	for ( char const& c : s )
	{
		gSerialBuffer.push( c );
	}
}

/**
 * Print a newline.
 */
static void bufferPrintln()
{
	bufferPrint( "\n" );
}

/**
 * Print a string, then a newline.
 */
static void bufferPrintln( const String& s )
{
	bufferPrint( s );
	bufferPrint( "\n" );
}

/**
 * Print a 16-bit word as four hex nibbles, big-endian.
 */
static void bufferPrintHex( uint16_t value )
{
	gSerialBuffer.push( wordToHex( value, 3 ) );
	gSerialBuffer.push( wordToHex( value, 2 ) );
	gSerialBuffer.push( wordToHex( value, 1 ) );
	gSerialBuffer.push( wordToHex( value, 0 ) );
}

/**
 * Print an 8-bit byte as two hex nibbles, big-endian.
 */
static void bufferPrintHex2( uint8_t value )
{
	gSerialBuffer.push( wordToHex( value, 1 ) );
	gSerialBuffer.push( wordToHex( value, 0 ) );
}

/**
 * Convert 4 bits from an integer to a single hex nibble.
 */
static char wordToHex( uint16_t value, uint8_t nibble_idx )
{
	static const char hexNibbles[] = { '0', '1', '2', '3', '4', '5', '6', '7',
	                                   '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	uint8_t nibble = ( value >> ( 4 * nibble_idx ) ) & 0x000F;
	return hexNibbles[nibble];
}
