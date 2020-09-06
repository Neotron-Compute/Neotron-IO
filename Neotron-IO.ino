/**
 * Neotron-IO Firmware.
 *
 * Uses MiniCore for the AtMega328P. Set to 8 MHz Internal RC. Don't go higher
 * than 9600 baud.
 *
 * This project is also licensed under the GPL (v3 or later version, at your
 * choice). See the [LICENCE](./LICENCE) file.
 */

//
// Includes
//
#include <EEPROM.h>

#include "RingBuf.h"
#include "hid.h"
#include "joystick.h"
#include "nonstd.h"
#include "ps2.h"

// Required for nonstd.h to work
void* operator new( size_t size, void* ptr )
{
	return ptr;
}

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
const int MS_DAT = A3;
const int MS_CLK = A2;
const int KB_DAT = A1;
const int KB_CLK = A0;

const uint8_t EEPROM_MAGIC_BYTE = 0xE0;
const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_OSCCAL = 1;

const uint16_t DEBOUNCE_LOOPS = 5;

const size_t MAX_INPUT_BUFFER = 16;

/**
 * This is the default `Boot Keyboard` HID descriptor
 * from a USB device.
 */
static const HIDReportShortDescriptorElement hid_keyboard_report[] = {
    // Header
    HIDReportShortDescriptorElement::UsagePage(
        HidUsagePageId::GENERIC_DESKTOP ),
    HIDReportShortDescriptorElement::UsageId(
        HidGenericDesktopUsageId::KEYBOARD ),
    HIDReportShortDescriptorElement::Collection(
        HidCollectionType::APPLICATION ),
    // Modifier keys Byte (Left-Alt, Right-Ctrl, etc)
    HIDReportShortDescriptorElement::ReportSize( 1 ),
    HIDReportShortDescriptorElement::ReportCount( 8 ),
    HIDReportShortDescriptorElement::UsagePage(
        HidUsagePageId::KEYBOARD_KEYPAD ),
    HIDReportShortDescriptorElement::UsageMinimum( 0xE0 ),
    HIDReportShortDescriptorElement::UsageMaximum( 0xE7 ),
    HIDReportShortDescriptorElement::LogicalMinimum( 0 ),
    HIDReportShortDescriptorElement::LogicalMaximum( 1 ),
    HIDReportShortDescriptorElement::Input( false /* is_const */,
                                            true /* is_variable */,
                                            false /* is_relative */,
                                            false /* is_wrap */,
                                            false /* is_non_linear */,
                                            false /* no_preferred */,
                                            false /* null_state */,
                                            false /* is_buffered_bytes */ ),
    // Reserved Byte
    HIDReportShortDescriptorElement::ReportSize( 8 ),
    HIDReportShortDescriptorElement::ReportCount( 1 ),
    HIDReportShortDescriptorElement::Input( true /* is_const */,
                                            false /* is_variable */,
                                            false /* is_relative */,
                                            false /* is_wrap */,
                                            false /* is_non_linear */,
                                            false /* no_preferred */,
                                            false /* null_state */,
                                            false /* is_buffered_bytes */ ),
    // LED Report (Num Lock, Caps Lock, Scroll Lock)
    HIDReportShortDescriptorElement::ReportCount( 3 ),
    HIDReportShortDescriptorElement::ReportSize( 1 ),
    HIDReportShortDescriptorElement::UsagePage( HidUsagePageId::LEDS ),
    HIDReportShortDescriptorElement::UsageMinimum( 1 ),
    HIDReportShortDescriptorElement::UsageMaximum( 3 ),
    HIDReportShortDescriptorElement::Output( false /* is_const */,
                                             true /* is_variable */,
                                             false /* is_relative */,
                                             false /* is_wrap */,
                                             false /* is_non_linear */,
                                             false /* no_preferred */,
                                             false /* null_state */,
                                             false /* is_volatile */,
                                             false /* is_buffered_bytes */ ),
    // LED report padding (to make it up to a byte)
    HIDReportShortDescriptorElement::ReportCount( 1 ),
    HIDReportShortDescriptorElement::ReportSize( 5 ),
    HIDReportShortDescriptorElement::Output( true /* is_const */,
                                             false /* is_variable */,
                                             false /* is_relative */,
                                             false /* is_wrap */,
                                             false /* is_non_linear */,
                                             false /* no_preferred */,
                                             false /* null_state */,
                                             false /* is_volatile */,
                                             false /* is_buffered_bytes */ ),
    // Keycodes for keys currently being pressed
    HIDReportShortDescriptorElement::ReportCount( 6 ),
    HIDReportShortDescriptorElement::ReportSize( 8 ),
    HIDReportShortDescriptorElement::LogicalMinimum( 0 ),
    HIDReportShortDescriptorElement::LogicalMaximum( 255 ),
    HIDReportShortDescriptorElement::UsagePage(
        HidUsagePageId::KEYBOARD_KEYPAD ),
    HIDReportShortDescriptorElement::UsageMinimum( 0 ),
    HIDReportShortDescriptorElement::UsageMaximum( 101 ),
    HIDReportShortDescriptorElement::Input( false /* is_const */,
                                            false /* is_variable */,
                                            false /* is_relative */,
                                            false /* is_wrap */,
                                            false /* is_non_linear */,
                                            false /* no_preferred */,
                                            false /* null_state */,
                                            false /* is_buffered_bytes */ ),
    HIDReportShortDescriptorElement::EndCollection(),
};

//
// Variables
//

static Joystick gJs1( JS1_PIN_UP,
                      JS1_PIN_DOWN,
                      JS1_PIN_GND_LEFT,
                      JS1_PIN_GND_RIGHT,
                      JS1_PIN_AB,
                      JS1_PIN_START_C,
                      JS1_PIN_SELECT );
static Joystick gJs2( JS2_PIN_UP,
                      JS2_PIN_DOWN,
                      JS2_PIN_GND_LEFT,
                      JS2_PIN_GND_RIGHT,
                      JS2_PIN_AB,
                      JS2_PIN_START_C,
                      JS2_PIN_SELECT );
static Ps2 gKeyboard( KB_CLK, KB_DAT );
static Ps2 gMouse( MS_CLK, MS_DAT );
static RingBuf<char, 256> gSerialBuffer;
static bool gCalibrationMode = 0;

// TODO make this a property of the HID controller object.
static const HidDescriptor hidDescriptor(
    /* _wReportDescLength */ 0,
    /* _wReportDescRegister */ 0,
    /* _wInputRegister */ 1,
    /* _wMaxInputLength */ 0,
    /* _wOutputRegister */ 2,
    /* _wMaxOutputLength */ 0,
    /* _wCommandRegister */ 3,
    /* _wDataRegister */ 4,
    /* _wVendorID */ 0xBEEF,
    /* _wProductID */ 0xDEAD,
    /* _wVersionID */ 0x0001 );

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
	pinMode( CALIBRATION_ON, INPUT_PULLUP );
	pinMode( CALIBRATION_OUT, OUTPUT );
	// Pull this pin low on reset to enter calibration mode
	gCalibrationMode = ( digitalRead( CALIBRATION_ON ) == 0 );
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
			break;
	}
}

// the loop function runs over and over again forever
void loop()
{
	static uint16_t debounce_count = 0;
	JoystickResult js1_bits;
	JoystickResult js2_bits;

	if ( Serial.available() )
	{
		char inputChar = Serial.read();
		processInput( inputChar );
	}

	if ( Serial.availableForWrite() )
	{
		char data;
		if ( gSerialBuffer.pop( data ) )
		{
			Serial.write( data );
		}
	}

	gKeyboard.poll();
	int keyboardByte = gKeyboard.readBuffer();
	if ( keyboardByte > 0 )
	{
		bufferPrint( "K" );
		bufferPrintHex2( keyboardByte );
		bufferPrintln();
	}

	gMouse.poll();
	int mouseByte = gKeyboard.readBuffer();
	if ( mouseByte > 0 )
	{
		bufferPrint( "M" );
		bufferPrintHex2( mouseByte );
		bufferPrintln();
	}

	if ( gJs1.scan() )
	{
		js1_bits = gJs1.read();
		bufferPrint( "J" );
		bufferPrintHex( js1_bits.value() );
		bufferPrintln();
	}

	if ( gJs2.scan() )
	{
		js2_bits = gJs2.read();
		bufferPrint( "K" );
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
 * Print a 16-bit word as four hex nibbles.
 */
static void bufferPrintHex( uint16_t value )
{
	gSerialBuffer.push( wordToHex( value, 0 ) );
	gSerialBuffer.push( wordToHex( value, 1 ) );
	gSerialBuffer.push( wordToHex( value, 2 ) );
	gSerialBuffer.push( wordToHex( value, 3 ) );
}

/**
 * Print an 8-bit byte as two hex nibbles.
 */
static void bufferPrintHex2( uint8_t value )
{
	gSerialBuffer.push( wordToHex( value, 0 ) );
	gSerialBuffer.push( wordToHex( value, 1 ) );
}

/**
 * Convert 4 bits from an integer to a single hex nibble.
 */
static char wordToHex( uint16_t value, uint8_t nibble_idx )
{
	static const char hexNibbles[] = { '0', '1', '2', '3', '4', '5', '6', '7',
	                                   '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
	uint8_t nibble = ( value >> ( 4 * ( 3 - nibble_idx ) ) ) & 0x000F;
	gSerialBuffer.push( hexNibbles[nibble] );
}
