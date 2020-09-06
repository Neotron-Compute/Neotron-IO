/**
 * Neotron-IO HID over I2C Implementation.
 *
 * Implements the HID Protocol so that it can be used over I2C (or UART).
 *
 * The Neotron-IO device exposes multiple top-level collections for Input:
 *
 * 1. Keyboard
 * 2. Mouse
 * 3. Joystick 1
 * 4. Joystick 2
 * 5. Buttons
 *
 * The Neotron-IO device exposes multiple top-level collections for Output:
 *
 * 1. Keyboard LEDs
 * 2. System LEDs
 *
 * This project is also licensed under the GPL (v3 or later version, at your
 * choice). See the [LICENCE](./LICENCE) file.
 */

#include <stddef.h>
#include <stdint.h>
#ifdef __AVR__
#include "nonstd.h"
#define std nonstd
#else
#include <functional>
#endif

/// Get the first byte of a uint16_t (the LSB)
static constexpr uint8_t first( uint16_t value )
{
	return ( value & 0x00FF );
}

/// Get the second byte of a uint16_t (the MSB)
static constexpr uint8_t second( uint16_t value )
{
	return ( ( value & 0xFF00 ) >> 8 );
}

/**
 * Describes a block of data that can be encoded.
 */
class HidEncodeable
{
   public:
	/// Encode this descriptor into this buffer.
	///
	/// @param p_buffer to the buffer to write the bytes to
	/// @param buffer_len the number of bytes available
	/// @return the number of bytes this function wanted to encode (may be
	///     larger than the number of bytes actually encoded if `buffer_len` is
	///     too small).
	virtual size_t encode_into_buffer( uint8_t* p_buffer, size_t buffer_len )
	{
		std::function<void( uint8_t )> encode_fn =
		    [&p_buffer, &buffer_len]( uint8_t byte ) noexcept {
			    if ( buffer_len > 0 )
			    {
				    *p_buffer++ = byte;
				    buffer_len--;
			    }
		    };
		return encode_into_fn( encode_fn );
	}

	/// Encode this descriptor using this function.
	///
	/// @param fn this function is called with each byte in turn.
	/// @return the number of bytes encoded
	virtual size_t encode_into_fn(
	    std::function<void( uint8_t )> encode_fn ) = 0;
};

/**
 * The HID Descriptor. This is supplied when the host reads the HID Descriptor
 * Register.
 *
 * Fields are transferred over the wire in little-endian format.
 */
struct HidDescriptor : HidEncodeable
{
	/// Length of this descriptor, when encoded.
	static const size_t LENGTH = 30;

	/// The length, in unsigned bytes, of the complete HID Descriptor. Should be
	/// 0x1E (30).
	uint16_t wHIDDescLength;
	/// The version number, in binary coded decimal (BCD) format. DEVICE should
	/// default to 0x0100.
	uint16_t bcdVersion;
	/// The length, in unsigned bytes, of the Report Descriptor. The maximum is
	/// 65535 bytes.
	uint16_t wReportDescLength;
	/// The register index containing the Report Descriptor on the DEVICE. Must
	/// be non-zero.
	uint16_t wReportDescRegister;
	/// This field identifies the register number to read the input report
	/// from the DEVICE. Must be non-zero.
	uint16_t wInputRegister;
	/// This field identifies, in unsigned bytes, the length of the largest
	/// Input Report to be read from the Input Register (Complex HID Devices
	/// will need various sized reports).
	uint16_t wMaxInputLength;
	/// This field identifies the register number to send the output report to
	/// the DEVICE.
	uint16_t wOutputRegister;
	/// This field identifies, in unsigned bytes, the length of the largest
	/// output Report to be sent to the Output Register (Complex HID Devices
	/// will need various sized reports).
	uint16_t wMaxOutputLength;
	/// This field identifies the register number to send command requests to
	/// the DEVICE.
	uint16_t wCommandRegister;
	/// This field identifies the register number to exchange data with the
	/// Command Request.
	uint16_t wDataRegister;
	/// This field identifies the DEVICE manufacturers Vendor ID. Must be
	/// non-zero.
	uint16_t wVendorID;
	/// This field identifies the DEVICE’s unique model / Product ID.
	uint16_t wProductID;
	/// This field identifies the DEVICE’s firmware revision number.
	uint16_t wVersionID;

	HidDescriptor( uint16_t _wReportDescLength,
	               uint16_t _wReportDescRegister,
	               uint16_t _wInputRegister,
	               uint16_t _wMaxInputLength,
	               uint16_t _wOutputRegister,
	               uint16_t _wMaxOutputLength,
	               uint16_t _wCommandRegister,
	               uint16_t _wDataRegister,
	               uint16_t _wVendorID,
	               uint16_t _wProductID,
	               uint16_t _wVersionID )
	{
		this->wHIDDescLength = LENGTH;
		this->bcdVersion = 0x0100;
		this->wReportDescLength = _wReportDescLength;
		this->wReportDescRegister = _wReportDescRegister;
		this->wInputRegister = _wInputRegister;
		this->wMaxInputLength = _wMaxInputLength;
		this->wOutputRegister = _wOutputRegister;
		this->wMaxOutputLength = _wMaxOutputLength;
		this->wCommandRegister = _wCommandRegister;
		this->wDataRegister = _wDataRegister;
		this->wVendorID = _wVendorID;
		this->wProductID = _wProductID;
		this->wVersionID = _wVersionID;
	}

	/// Encode this descriptor using this function.
	///
	/// @param fn this function is called with each byte in turn.
	/// @return the number of bytes encoded
	virtual size_t encode_into_fn( std::function<void( uint8_t )> encode_fn )
	{
		encode_fn( first( wHIDDescLength ) );
		encode_fn( second( wHIDDescLength ) );
		encode_fn( first( bcdVersion ) );
		encode_fn( second( bcdVersion ) );
		encode_fn( first( wReportDescLength ) );
		encode_fn( second( wReportDescLength ) );
		encode_fn( first( wReportDescRegister ) );
		encode_fn( second( wReportDescRegister ) );
		encode_fn( first( wInputRegister ) );
		encode_fn( second( wInputRegister ) );
		encode_fn( first( wMaxInputLength ) );
		encode_fn( second( wMaxInputLength ) );
		encode_fn( first( wOutputRegister ) );
		encode_fn( second( wOutputRegister ) );
		encode_fn( first( wMaxOutputLength ) );
		encode_fn( second( wMaxOutputLength ) );
		encode_fn( first( wCommandRegister ) );
		encode_fn( second( wCommandRegister ) );
		encode_fn( first( wDataRegister ) );
		encode_fn( second( wDataRegister ) );
		encode_fn( first( wVendorID ) );
		encode_fn( second( wVendorID ) );
		encode_fn( first( wProductID ) );
		encode_fn( second( wProductID ) );
		encode_fn( first( wVersionID ) );
		encode_fn( second( wVersionID ) );
		encode_fn( 0 );
		encode_fn( 0 );
		encode_fn( 0 );
		encode_fn( 0 );
		return LENGTH;
	}
};

/**
 * The set of HID Command Op-Codes we support.
 */
enum class HidCommandOpcode
{
	/// Reset the device at any time
	OPCODE_RESET = 1,
	/// Request from HOST to DEVICE to retrieve a report (either Input or
	/// Feature).
	OPCODE_GET_REPORT = 2,
	/// Request from HOST to DEVICE to set a report (either Output or Feature).
	OPCODE_SET_REPORT = 3,
	/// Request from HOST to DEVICE to retrieve the current idle rate for a
	/// particular top-level collection. This command is not used on modern
	/// HOSTS.
	OPCODE_GET_IDLE = 4,
	/// Request from HOST to DEVICE to set the current idle rate for a
	/// particular top-level collection. This command is not used on modern
	/// HOSTS.
	OPCODE_SET_IDLE = 5,
	/// Request from HOST to DEVICE to retrieve the protocol mode the device is
	/// operating in. This command is not used on modern HOSTS.
	OPCODE_GET_PROTOCOL = 6,
	/// Request from HOST to DEVICE to set the protocol mode the device should
	/// be operating in. This command is not used on modern HOSTS.
	OPCODE_SET_PROTOCOL = 7,
	/// Request from HOST to DEVICE to indicate preferred power setting.
	OPCODE_SET_POWER = 8,
};

/**
 * A HID Command must be about one of these types of HID Report.
 */
enum class HidCommandReportType
{
	REPORT_TYPE_RESERVED = 0,
	REPORT_TYPE_INPUT = 1,
	REPORT_TYPE_OUTPUT = 2,
	REPORT_TYPE_FEATURE = 3,
};

/**
 * These commands are written to the register described by
 * `HidDescriptor::wCommandRegister`.
 */
struct HidCommand : HidEncodeable
{
	/// The opcode field
	uint8_t opcode;
	/// The report-type and report-ID field.
	uint8_t type_id;

	static const size_t LENGTH = 2;

	/**
	 * Create a HID Command that can be sent to the register described in
	 * `HidDescriptor::wCommandRegister`.
	 *
	 * @param opcode the op-code of the command to send
	 * @param report_type the type of report this command is about
	 * @param report_id the report ID this command is about
	 */
	HidCommand( HidCommandOpcode _opcode,
	            HidCommandReportType _report_type,
	            uint8_t _report_id )
	{
		opcode = static_cast<uint8_t>( _opcode ),
		type_id = static_cast<uint8_t>(
		    ( static_cast<uint8_t>( _report_type ) << 4 ) +
		    ( _report_id & 0x0F ) );
	};

	/// Encode this descriptor using this function.
	///
	/// @param fn this function is called with each byte in turn.
	/// @return the number of bytes encoded
	virtual size_t encode_into_fn( std::function<void( uint8_t )> encode_fn )
	{
		encode_fn( opcode );
		encode_fn( type_id );
		return LENGTH;
	}
};

/**
 * The Report Descriptor element tags that we support (as part of a
 * tag-length-value element).
 */
enum class HidReportTag
{
	/// Refers to the data from one or more similar controls on a device. For
	/// example, variable data such as reading the position of a single axis
	/// or a group of levers or array data such as one or more push buttons or
	/// switches.
	MAIN_INPUT = 8,
	/// Refers to the data to one or more similar controls on a device such as
	/// setting the position of a single axis or a group of levers (variable
	/// data). Or, it can represent data to one or more LEDs (array data).
	MAIN_OUTPUT = 9,
	/// Describes device input and output not intended for consumption by the
	/// end user — for example, a software feature or Control Panel toggle.
	MAIN_FEATURE = 11,
	/// Describes a meaningful grouping of Input, Output and Feature items.
	MAIN_COLLECTION = 10,
	/// A terminating item used to specify the end of a collection of items.
	MAIN_END_COLLECTION = 12,
	/// Unsigned integer specifying the current Usage Page. Since a usage are
	/// 32 bit values, Usage Page items can be used to conserve space in a
	/// report descriptor by setting the high order 16 bits of a subsequent
	/// usages. Any usage that follows which is defines 16 bits or less is
	/// interpreted as a Usage ID and concatenated with the Usage Page to form
	/// a 32 bit Usage.
	GLOBAL_USAGE_PAGE = 0,
	/// Extent value in logical units. This is the minimum value that a
	/// variable or array item will report. For example, a mouse reporting x
	/// position values from 0 to 128 would have a Logical Minimum of 0 and a
	/// Logical Maximum of 128.
	GLOBAL_LOGICAL_MINIMUM = 1,
	/// Extent value in logical units. This is the maximum value that a
	/// variable or array item will report.
	GLOBAL_LOGICAL_MAXIMUM = 2,
	/// Minimum value for the physical extent of a variable item.
	GLOBAL_PHYSICAL_MINIMUM = 3,
	/// Maximum value for the physical extent of a variable item.
	GLOBAL_PHYSICAL_MAXIMUM = 4,
	/// Value of the unit exponent in base 10. E.g. -3 for milli.
	GLOBAL_UNIT_EXPONENT = 5,
	/* clang-format off */
	/// Unit values.
	///
	/// | Nibble | System             | 0x0  | 0x1        | 0x2         | 0x3            | 0x4              |
	/// |:-------|:-------------------|:-----|:-----------|:------------|:---------------|:-----------------|
	/// | 0      | System             | None | SI Linear  | SI Rotation | English Linear | English Rotation |
	/// | 1      | Length             | None | Centimeter | Radians     | Inch           | Degrees          |
	/// | 2      | Mass               | None | Gram       | Gram        | Slug           | Slug             |
	/// | 3      | Time               | None | Seconds    | Seconds     | Seconds        | Seconds          |
	/// | 4      | Temperature        | None | Kelvin     | Kelvin      | Farenheit      | Farenheit        |
	/// | 5      | Current            | None | Ampere     | Ampere      | Ampere         | Ampere           |
	/// | 6      | Luminous Intensity | None | Candela    | Candela     | Candela        | Candela          |
	/// | 7      | Reserved           | None | None       | None        | None           | None             |
	///
	/// Each nibble value gives the exponent for that unit:
	///
	/// * 0..7 = 10^0 .. 10^7
	/// * 8..15 = 10^-8 .. 10^-1
	///
	/// Examples:
	/// * Distance (cm) = 0x0000_0011
	/// * Time (seconds) = 0x0000_1001
	/// * Velocity (cm per second) = 0x000_0F011
	/// * Energy (100 nJ) = 0x0000_E121
	/// * Voltage (100 nV) = 0x00F0_D121
	/* clang-format on */
	GLOBAL_UNIT = 6,
	/// Unsigned integer specifying the size of thereportfields in bits. This
	/// allows the parser to build an item map for the report handler to use.
	GLOBAL_REPORT_SIZE = 7,
	/// Unsigned value that specifies the Report ID. If a Report ID tag is
	/// used anywhere in Report descriptor, all data reports for the device
	/// are preceded by a single byte ID field. All items succeeding the first
	/// Report ID tag but preceding a second Report ID tag are included in a
	/// report prefixed by a 1-byte ID. All items succeeding the second but
	/// preceding a third Report ID tag are included in a second report
	/// prefixed by a second ID, and soon.
	GLOBAL_REPORT_ID = 8,
	/// Unsigned integer specifying the number of data fields for the item;
	/// determines how many fields are included in the report for this
	/// particular item (and consequently how many bits are added to the
	/// report).
	GLOBAL_REPORT_COUNT = 9,
	/// Places a copy of the global item state table on the stack.
	GLOBAL_PUSH = 10,
	/// Replaces the item state table with the top structure from the stack.
	GLOBAL_POP = 11,
	/// Usage index for an item usage; represents a suggested usage for the item
	/// or collection. In the case where an item represents multiple controls, a
	/// Usage tag may suggest a usage for every variable or element in an array.
	LOCAL_USAGE = 0,
	// Defines the starting usage associated with an array or bitmap.
	LOCAL_USAGE_MINIMUM = 1,
	// Defines the ending usage associated with an array or bitmap.
	LOCAL_USAGE_MAXIMUM = 2,
	/// Determines the body part used for a control. Index points to a
	/// designator in the Physical descriptor.
	LOCAL_DESIGNATOR_INDEX = 3,
	/// Defines the index of the starting designator associated with an array or
	/// bitmap.
	LOCAL_DESIGNATOR_MINIMUM = 4,
	/// Defines the index of the ending designator associated with an array or
	/// bitmap.
	LOCAL_DESIGNATOR_MAXIMUM = 5,
	/// String index for a String descriptor; allows a stringto be associated
	/// with a particular item or control.
	LOCAL_STRING_INDEX = 7,
	/// Specifies the first string index when assigning a group of sequential
	/// strings to controls in an array or bitmap.
	LOCAL_STRING_MINIMUM = 8,
	/// Specifies the last string index when assigning a group of sequential
	/// strings to controls in an array or bitmap.
	LOCAL_STRING_MAXIMUM = 9,
	/// Defines the beginning or end of a set of localitems (1 = open set, 0 =
	/// close set)
	LOCAL_DELIMITER = 10,
};

/**
 * The types of HID Report Element we support.
 */
enum class HidReportType
{
	/// An Input, Output or Feature item.
	MAIN = 0,
	/// Defines properties for all items.
	GLOBAL = 1,
	/// Defines properties for the next item.
	LOCAL = 2,
};

enum class HidReportSize
{
	ZERO_BYTES = 0,
	ONE_BYTE = 1,
	TWO_BYTES = 2,
	FOUR_BYTES = 3,
};

/**
 * Types of collection supported by HID.
 */
enum class HidCollectionType
{
	/// A physical collection is used for a set of data items that represent
	/// data points collected at one geometric point. This is useful for
	/// sensing devices which may need to associate sets of measured or sensed
	/// data with a single point. It does not indicate that a set of data
	/// values comes from one device, such as a keyboard. In the case of
	/// device which reports the position of multiple sensors, physical
	/// collections are used to show which data comes from each separate
	/// sensor.
	PHYSICAL = 0,
	/// A group of Main items that might be familiar to applications. Common
	/// examples are a keyboardor mouse. A keyboard with an integrated
	/// pointing device could be defined as two different application
	/// collections. Data reports are usually (but not necessarily) associated
	/// with application collections (at least one report ID per application).
	APPLICATION = 1,
	/// A logical collection is used when a set of data items form a composite
	/// data structure. An example of this is the association between a data
	/// buffer and a byte count of the data. The collection establishes the
	/// link between the count and the buffer.
	LOGICAL = 2,
	/// Defines a logical collection that wraps all the fields in a report. A
	/// unique report ID will be contained in this collection. An application
	/// can easily determine whether a device supports a certain function.
	/// Note that any valid Report ID value can be declared for a Report
	/// collection.
	REPORT = 3,
	/// A named array is a logical collection contains an array of selector
	/// usages. For a given function the set of selectors used by similar
	/// devices may vary. The naming of fields is common practice when
	/// documenting hardware registers. To determine whether a device supports
	/// a particular function like Status, an application might have to query
	/// for several known Status selector usages before it could determine
	/// whether the device supported Status. The Named Array usages allows the
	/// Array field that contains the selectors to be named, thus the
	/// application only needs to query for the Status usage to determine that
	/// a device supports status information.
	NAMED_ARRAY = 4,
	/// A Usage Switch is a logical collection that modifies the meaning of
	/// the usages that it contains. This collection type indicates to an
	/// application that the usages found in this collection must be special
	/// cased. For instance, rather than declaring a usage on the LED page for
	/// every possible function, an Indicator usage can be applied to a Usage
	/// Switch collection and the standard usages defined in that collection
	/// can now be identified as indicators for a function rather than the
	/// function itself. Note that this collection type is not used for the
	/// labeling Ordinal collections, a Logical collection type is used for
	/// that.
	USAGE_SWITCH = 5,
	/// Modifies the meaning of the usage attached to the encompassing
	/// collection. A usage typically defines a single operating mode for a
	/// control. The usage modifier allows the operating mode of a control to
	/// be extended. For instance, an LED is typically on or off. For
	/// particular states a device may want a generic method of blinking or
	/// choosing the color of a standard LED. Attaching the LED usage to a
	/// Usage Modifier collection will indicate to an application that the
	/// usage supports a new operating mode.
	USAGE_MODIFIER = 6,
};

/**
 * HID Usage Page IDs.
 */
enum class HidUsagePageId
{
	UNDEFINED = 0x00,
	GENERIC_DESKTOP = 0x01,
	SIMULATION_CONTROLS = 0x02,
	VR_CONTROLS = 0x03,
	SPORT_CONTROLS = 0x04,
	GAME_CONTROLS = 0x05,
	GENERIC_DEVICE_CONTROLS = 0x06,
	KEYBOARD_KEYPAD = 0x07,
	LEDS = 0x08,
	BUTTONS = 0x09,
	ORDINAL = 0x0A,
	TELEPHONY = 0x0B,
	CONSUMER = 0x0C,
	DIGITIZER = 0x0D,
	PHYSICAL_INTERFACE_DEVICE = 0x0F,
	UNICODE = 0x10,
	ALPHANUMERIC_DISPLAY = 0x14,
	MEDICAL_INSTRUMENT = 0x40,
	MONITOR0 = 0x80,
	MONITOR1 = 0x81,
	MONITOR2 = 0x82,
	MONITOR3 = 0x83,
	POWER0 = 0x84,
	POWER1 = 0x85,
	POWER2 = 0x86,
	POWER3 = 0x87,
	BAR_CODE_SCANNER = 0x8C,
	SCALES = 0x8D,
	MAGNETIC_STRIP_READER = 0x8E,
	POINT_OF_SALE = 0x8F,
	CAMERA_CONTROL = 0x90,
	ARCADE = 0x91,
};

/**
 * Usage IDs for the `HidUsagePageId::GENERIC_DESKTOP` usage page.
 */
enum class HidGenericDesktopUsageId : uint16_t
{
	/// Pointer
	POINTER = 0x01,
	/// Mouse
	MOUSE = 0x02,
	/// Reserved
	RESERVED = 0x03,
	/// Joystick
	JOYSTICK = 0x04,
	/// Game Pad
	GAME_PAD = 0x05,
	/// Keyboard
	KEYBOARD = 0x06,
	/// Keypad
	KEYPAD = 0x07,
	/// Multi-axis Controller
	MULTI_AXIS_CONTROLLER = 0x08,
	/// Tablet PC System Controls
	TABLET_PC_SYSTEM_CONTROLS = 0x09,
	/// X
	X = 0x30,
	/// Y
	Y = 0x31,
	/// Z
	Z = 0x32,
	/// Rx
	RX = 0x33,
	/// Ry
	RY = 0x34,
	/// Rz
	RZ = 0x35,
	/// Slider
	SLIDER = 0x36,
	/// Dial
	DIAL = 0x37,
	/// Wheel
	WHEEL = 0x38,
	/// Hat switch
	HAT_SWITCH = 0x39,
	/// Counted BufferCL
	COUNTED_BUFFERCL = 0x3A,
	/// Byte Count
	BYTE_COUNT = 0x3B,
	/// Motion Wakeup
	MOTION_WAKEUP = 0x3C,
	/// Start
	START = 0x3D,
	/// Select
	SELECT = 0x3E,
	/// Vx
	VX = 0x40,
	/// Vy
	VY = 0x41,
	/// Vz
	VZ = 0x42,
	/// Vbrx
	VBRX = 0x43,
	/// Vbry
	VBRY = 0x44,
	/// Vbrz
	VBRZ = 0x45,
	/// Vno
	VNO = 0x46,
	/// Feature Notification
	FEATURE_NOTIFICATION = 0x47,
	/// Resolution Multiplier
	RESOLUTION_MULTIPLIER = 0x48,
	/// System Control
	SYSTEM_CONTROL = 0x80,
	/// System Power Down
	SYSTEM_POWER_DOWN = 0x81,
	/// System Sleep
	SYSTEM_SLEEP = 0x82,
	/// System Wake Up
	SYSTEM_WAKE_UP = 0x83,
	/// System Context Menu
	SYSTEM_CONTEXT_MENU = 0x84,
	/// System Main Menu
	SYSTEM_MAIN_MENU = 0x85,
	/// System App Menu
	SYSTEM_APP_MENU = 0x86,
	/// System Menu Help
	SYSTEM_MENU_HELP = 0x87,
	/// System Menu Exit
	SYSTEM_MENU_EXIT = 0x88,
	/// System Menu Select
	SYSTEM_MENU_SELECT = 0x89,
	/// System Menu Right
	SYSTEM_MENU_RIGHT = 0x8A,
	/// System Menu Left
	SYSTEM_MENU_LEFT = 0x8B,
	/// System Menu Up
	SYSTEM_MENU_UP = 0x8C,
	/// System Menu Down
	SYSTEM_MENU_DOWN = 0x8D,
	/// System Cold Restart
	SYSTEM_COLD_RESTART = 0x8E,
	/// System Warm Restart
	SYSTEM_WARM_RESTART = 0x8F,
	/// D-pad Up
	D_PAD_UP = 0x90,
	/// D-pad Down
	D_PAD_DOWN = 0x91,
	/// D-pad Right
	D_PAD_RIGHT = 0x92,
	/// D-pad Left
	D_PAD_LEFT = 0x93,
	/// System Dock
	SYSTEM_DOCK = 0xA0,
	/// System Undock
	SYSTEM_UNDOCK = 0xA1,
	/// System Setup
	SYSTEM_SETUP = 0xA2,
	/// System Break
	SYSTEM_BREAK = 0xA3,
	/// System Debugger Break
	SYSTEM_DEBUGGER_BREAK = 0xA4,
	/// Application Break
	APPLICATION_BREAK = 0xA5,
	/// Application Debugger Break
	APPLICATION_DEBUGGER_BREAK = 0xA6,
	/// System Speaker Mute
	SYSTEM_SPEAKER_MUTE = 0xA7,
	/// System Hibernate
	SYSTEM_HIBERNATE = 0xA8,
	/// System Display Invert
	SYSTEM_DISPLAY_INVERT = 0xB0,
	/// System Display Internal
	SYSTEM_DISPLAY_INTERNAL = 0xB1,
	/// System Display External
	SYSTEM_DISPLAY_EXTERNAL = 0xB2,
	/// System Display Both
	SYSTEM_DISPLAY_BOTH = 0xB3,
	/// System Display Dual
	SYSTEM_DISPLAY_DUAL = 0xB4,
	/// System Display Toggle Int/Ext
	SYSTEM_DISPLAY_TOGGLE_INT_EXT = 0xB5,
	/// System Display Swap Primary/Secondary
	SYSTEM_DISPLAY_SWAP_PRIMARY_SECONDARY = 0xB6,
	/// System Display LCD Autoscale
	SYSTEM_DISPLAY_LCD_AUTOSCALE = 0xB7,
};

/*
We need to be able to encode a report descriptor like the following:

Usage Page (Generic Desktop)
Usage (Keyboard)
Collection (Application):
        // Modifier byte
        Local: Report Size (1)
        Local: Report Count (8)
        Local: Usage Page (Key Codes)
        Local: Usage Minimum (224)
        Local: Usage Maximum (231)
        Local: Logical Minimum (0)
        Local: Logical Maximum (1)
        ** Main: Input (Data, Variable, Absolute) **
        // Reserved byte
        Local: Report Count (1)
        Local: Report Size (8)
        ** Main: Input(Constant) **
        // LED report
        Local: Report Count (5)
        Local: Report Size (1)
        Local: Usage Page (LEDs)
        Local: Usage Minimum (1)
        Local: Usage Maximum (5)
        ** Main: Output (Data, Variable, Absolute) **
        // LED report padding
        Local: Report Count (1)
        Local: Report Size (3)
        ** Main: Output(Constant) **
        // Keycodes for pressed keys
        Local: Report Count (6)
        Local: Report Size (8)
        Local: Logical Minimum (0)
        Local: Logical Maximum (255)
        Local: Usage Page (Key Codes)
        Local: Usage Minimum (0)
        Local: Usage Maximum (255)
        ** Main: Input (Data, Array) **
End Collection
 */

/**
 * An item in the Report Descriptor, including the value.
 *
 * A Report Descriptor is made of these. The length is computed at run-time
 * based on the number of zero leading bits in the value.
 */
struct HIDReportShortDescriptorElement
{
	uint8_t header;
	uint32_t value;

	/**
	 * Pack this Short Descriptor into a byte array.
	 *
	 * The `value` field has variable length encoding.
	 *
	 * @param p_buffer where to write the encoded data
	 * @param space how much space in the `p_buffer` buffer
	 * @return how many bytes we wanted to write (whether or not they fitted)
	 */
	size_t pack( uint8_t* p_buffer, size_t space ) const
	{
		size_t space_needed = 1;
		uint8_t bSize = size_of_value( this->value );
		switch ( bSize )
		{
			case 3:
				space_needed += 4;
				break;
			case 2:
				space_needed += 2;
				break;
			case 1:
				space_needed += 1;
				break;
			default:
				break;
		}

		const size_t return_value = space_needed;

		if ( space >= 1 )
		{
			*p_buffer++ = header;
			space--;
			space_needed--;
		}

		size_t value_copy = value;
		while ( space-- && space_needed-- )
		{
			*p_buffer++ = value_copy & 0xFF;
			value_copy >>= 8;
		}

		return return_value;
	}

	/**
	 * How many bytes do we want to take up when packing `value`.
	 *
	 * @param value the value we want to think about packing
	 * @return the `bSize` for that value (3, 2, 1 or 0)
	 */
	static constexpr uint8_t size_of_value( uint32_t value )
	{
		return ( value >= 0x00010000 ) ? 3
		                               : ( ( value >= 0x000000FF ) ? 2 : 1 );
	}

	/**
	 * Build the object from a tag, type and a value.
	 */
	constexpr HIDReportShortDescriptorElement( HidReportTag tag,
	                                           HidReportType type,
	                                           uint32_t value )
	    : header( ( static_cast<uint8_t>( tag ) << 4 ) |
	              ( static_cast<uint8_t>( type ) << 2 ) |
	              size_of_value( value ) ),
	      value( value )
	{
	}

	/**
	 * Creates a Main Input element for a Report Descriptor.
	 *
	 * @param is_const Indicates whether the item is data or a constant value.
	 * @param is_variable Indicates whether the item creates variable or array
	 *     data fields in reports.
	 * @param is_relative Indicates whether the data is absolute (based on a
	 * fixed origin) or relative (indicating the change in value from the last
	 *     report).
	 * @param is_wrap Indicates whether the data “rolls over” when reaching
	 * either the extreme high or low value.
	 * @param is_non_linear Indicates whether the raw data from the devicehas
	 * been processed in some way, and no longer represents a linear
	 * relationship between what is measured and the data that is reported.
	 * @param no_preferred Indicates whether the control does not a preferred
	 *     state to which it will return when the user is not physically
	 *     interacting with the control.
	 * @param null_state Indicates whether the control has a state in which it
	 * is not sending meaningful data.
	 * @param is_buffered_bytes Indicates that the control emits a fixed-size
	 *     stream of bytes as opposed to a single numeric quantity.
	 */
	static constexpr HIDReportShortDescriptorElement Input(
	    bool is_const,
	    bool is_variable,
	    bool is_relative,
	    bool is_wrap,
	    bool is_non_linear,
	    bool no_preferred,
	    bool null_state,
	    bool is_buffered_bytes )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::MAIN_INPUT, HidReportType::MAIN,
		    ( ( is_const ) ? ( 1u << 0 ) : 0u ) +
		        ( ( is_variable ) ? ( 1u << 1 ) : 0u ) +
		        ( ( is_relative ) ? ( 1u << 2 ) : 0u ) +
		        ( ( is_wrap ) ? ( 1u << 3 ) : 0u ) +
		        ( ( is_non_linear ) ? ( 1u << 4 ) : 0u ) +
		        ( ( no_preferred ) ? ( 1u << 5 ) : 0u ) +
		        ( ( null_state ) ? ( 1u << 6 ) : 0u ) +
		        ( ( is_buffered_bytes ) ? ( 1u << 8 ) : 0u ) );
	}

	/**
	 * Creates a Main Output element for a Report Descriptor.
	 *
	 * @param is_const Indicates whether the item is data or a constant value.
	 * @param is_variable Indicates whether the item creates variable or array
	 *     data fields in reports.
	 * @param is_relative Indicates whether the data is absolute (based on a
	 * fixed origin) or relative (indicating the change in value from the last
	 *     report).
	 * @param is_wrap Indicates whether the data “rolls over” when reaching
	 * either the extreme high or low value.
	 * @param is_non_linear Indicates whether the raw data from the devicehas
	 * been processed in some way, and no longer represents a linear
	 * relationship between what is measured and the data that is reported.
	 * @param no_preferred Indicates whether the control does not a preferred
	 *     state to which it will return when the user is not physically
	 *     interacting with the control.
	 * @param null_state Indicates whether the control has a state in which it
	 * is not sending meaningful data.
	 * @param is_volatile Indicates whether the Output control's value should be
	 *     changed by the host or not.
	 * @param is_buffered_bytes Indicates that the control emits a fixed-size
	 *     stream of bytes as opposed to a single numeric quantity.
	 */
	static constexpr HIDReportShortDescriptorElement Output(
	    bool is_const,
	    bool is_variable,
	    bool is_relative,
	    bool is_wrap,
	    bool is_non_linear,
	    bool no_preferred,
	    bool null_state,
	    bool is_volatile,
	    bool is_buffered_bytes )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::MAIN_OUTPUT, HidReportType::MAIN,
		    ( ( is_const ) ? ( 1u << 0 ) : 0u ) +
		        ( ( is_variable ) ? ( 1u << 1 ) : 0u ) +
		        ( ( is_relative ) ? ( 1u << 2 ) : 0u ) +
		        ( ( is_wrap ) ? ( 1u << 3 ) : 0u ) +
		        ( ( is_non_linear ) ? ( 1u << 4 ) : 0u ) +
		        ( ( no_preferred ) ? ( 1u << 5 ) : 0u ) +
		        ( ( null_state ) ? ( 1u << 6 ) : 0u ) +
		        ( ( is_volatile ) ? ( 1u << 7 ) : 0u ) +
		        ( ( is_buffered_bytes ) ? ( 1u << 8 ) : 0u ) );
	}

	/**
	 * Creates a Main Feature element for a Report Descriptor.
	 *
	 * @param is_const Indicates whether the item is data or a constant value.
	 * @param is_variable Indicates whether the item creates variable or array
	 *     data fields in reports.
	 * @param is_relative Indicates whether the data is absolute (based on a
	 * fixed origin) or relative (indicating the change in value from the last
	 *     report).
	 * @param is_wrap Indicates whether the data “rolls over” when reaching
	 * either the extreme high or low value.
	 * @param is_non_linear Indicates whether the raw data from the devicehas
	 * been processed in some way, and no longer represents a linear
	 * relationship between what is measured and the data that is reported.
	 * @param no_preferred Indicates whether the control does not a preferred
	 *     state to which it will return when the user is not physically
	 *     interacting with the control.
	 * @param null_state Indicates whether the control has a state in which it
	 * is not sending meaningful data.
	 * @param is_volatile Indicates whether the Output control's value should be
	 *     changed by the host or not.
	 * @param is_buffered_bytes Indicates that the control emits a fixed-size
	 *     stream of bytes as opposed to a single numeric quantity.
	 */
	static constexpr HIDReportShortDescriptorElement Feature(
	    bool is_const,
	    bool is_variable,
	    bool is_relative,
	    bool is_wrap,
	    bool is_non_linear,
	    bool no_preferred,
	    bool null_state,
	    bool is_volatile,
	    bool is_buffered_bytes )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::MAIN_FEATURE, HidReportType::MAIN,
		    ( ( is_const ) ? ( 1u << 0 ) : 0u ) +
		        ( ( is_variable ) ? ( 1u << 1 ) : 0u ) +
		        ( ( is_relative ) ? ( 1u << 2 ) : 0u ) +
		        ( ( is_wrap ) ? ( 1u << 3 ) : 0u ) +
		        ( ( is_non_linear ) ? ( 1u << 4 ) : 0u ) +
		        ( ( no_preferred ) ? ( 1u << 5 ) : 0u ) +
		        ( ( null_state ) ? ( 1u << 6 ) : 0u ) +
		        ( ( is_volatile ) ? ( 1u << 7 ) : 0u ) +
		        ( ( is_buffered_bytes ) ? ( 1u << 8 ) : 0u ) );
	}

	/**
	 * Creates a Collection element for a Report Descriptor.
	 *
	 * A Collection item identifies a relationship between two or more data
	 * (Input, Output, or Feature.) For example, a mouse could be described as a
	 * collection of two to four data (x, y, button 1, button 2). While the
	 * Collection item opens a collection of data, the End Collection item
	 * closes a collection.
	 *
	 * @param collection_type Type of collection
	 */
	static constexpr HIDReportShortDescriptorElement Collection(
	    HidCollectionType collection_type )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::MAIN_COLLECTION, HidReportType::MAIN,
		    static_cast<uint32_t>( collection_type ) );
	}

	/**
	 * Creates an End Collection element for a Report Descriptor.
	 *
	 * An End Collection item closes a collection.
	 */
	static constexpr HIDReportShortDescriptorElement EndCollection( void )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::MAIN_END_COLLECTION, HidReportType::MAIN, 0 );
	}

	/**
	 * Creates an Global Usage Page element for a Report Descriptor.
	 *
	 * @param usage_page the usage page to set
	 *
	 */
	static constexpr HIDReportShortDescriptorElement UsagePage(
	    HidUsagePageId usage_page )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::GLOBAL_USAGE_PAGE, HidReportType::GLOBAL,
		    static_cast<uint16_t>( usage_page ) );
	}

	/**
	 * Creates an Local Usage ID element for a Report Descriptor.
	 *
	 * Any 16-bit Usage ID is assumed to belong to the previous Usage Page.
	 *
	 * @param usage_id the usage ID to set
	 */
	template <class T>
	static constexpr HIDReportShortDescriptorElement UsageId( T usage_id )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::LOCAL_USAGE, HidReportType::LOCAL,
		    static_cast<uint32_t>( usage_id ) );
	}

	/**
	 * Creates an Local Usage ID element for a Report Descriptor.
	 *
	 * Takes a Usage ID and a page.
	 *
	 * @param usage_id the usage ID to set
	 */
	template <class T>
	static constexpr HIDReportShortDescriptorElement UsageId(
	    HidUsagePageId page_id,
	    T usage_id )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::LOCAL_USAGE, HidReportType::LOCAL,
		    ( static_cast<uint32_t>( page_id ) << 16 ) |
		        static_cast<uint32_t>( usage_id ) );
	}

	/**
	 * Creates a ReportSize element for a Report Descriptor.
	 *
	 * Unsigned integer specifying the size of the report fields in bits. This
	 * allows the parser to build an item map for the report handler to use.
	 *
	 * @param size Size of an element, in bits.
	 */
	static constexpr HIDReportShortDescriptorElement ReportSize( uint32_t size )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::GLOBAL_REPORT_SIZE, HidReportType::GLOBAL, size );
	}

	/**
	 * Creates a ReportCount element for a Report Descriptor.
	 *
	 * Unsigned integer specifying the number of data fields for the item;
	 * determines how many fields are included in the report for this
	 * particular item (and consequently how many bits are added to the
	 * report).
	 *
	 * @param size Size of a report item, in elements.
	 */
	static constexpr HIDReportShortDescriptorElement ReportCount(
	    uint32_t count )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::GLOBAL_REPORT_COUNT, HidReportType::GLOBAL, count );
	}

	/**
	 * Creates a LogicalMinimum element for a Report Descriptor.
	 *
	 * Extent value in logical units. This is the minimum value that a variable
	 * or array item will report. For example, a mouse reporting x position
	 * values from 0 to 128 would have a Logical Minimum of 0 and a Logical
	 * Maximum of 128.
	 *
	 * @param units minimum value in logical units
	 */
	static constexpr HIDReportShortDescriptorElement LogicalMinimum(
	    uint32_t units )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::GLOBAL_LOGICAL_MINIMUM, HidReportType::GLOBAL,
		    units );
	}

	/**
	 * Creates a LogicalMaximum element for a Report Descriptor.
	 *
	 * Extent value in logical units. This is the maximum value that a variable
	 * or array item will report.
	 *
	 * @param units maximum value in logical units
	 */
	static constexpr HIDReportShortDescriptorElement LogicalMaximum(
	    uint32_t units )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::GLOBAL_LOGICAL_MAXIMUM, HidReportType::GLOBAL,
		    units );
	}

	/**
	 * Creates a PhysicalMinimum element for a Report Descriptor.
	 *
	 * Minimum value for the physical extent of a variable item.
	 *
	 * @param units minimum value in Physical units
	 */
	static constexpr HIDReportShortDescriptorElement PhysicalMinimum(
	    uint32_t units )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::GLOBAL_PHYSICAL_MINIMUM, HidReportType::GLOBAL,
		    units );
	}

	/**
	 * Creates a PhysicalMaximum element for a Report Descriptor.
	 *
	 * Maximum value for the physical extent of a variable item.
	 *
	 * @param units maximum value in Physical units
	 */
	static constexpr HIDReportShortDescriptorElement PhysicalMaximum(
	    uint32_t units )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::GLOBAL_PHYSICAL_MAXIMUM, HidReportType::GLOBAL,
		    units );
	}

	/**
	 * Creates a UsageMinimum element for a Report Descriptor.
	 *
	 * Defines the starting usage associated with an array or bitmap. Allows
	 * every element to be assigned a unique Usage ID.
	 *
	 * @param usage starting usage
	 */
	static constexpr HIDReportShortDescriptorElement UsageMinimum(
	    uint32_t usage )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::LOCAL_USAGE_MINIMUM, HidReportType::LOCAL, usage );
	}

	/**
	 * Creates a UsageMaximum element for a Report Descriptor.
	 *
	 * Defines the ending usage associated with an array or bitmap. Allows
	 * every element to be assigned a unique Usage ID.
	 *
	 * @param usage ending usage
	 */
	static constexpr HIDReportShortDescriptorElement UsageMaximum(
	    uint32_t usage )
	{
		return HIDReportShortDescriptorElement(
		    HidReportTag::LOCAL_USAGE_MAXIMUM, HidReportType::LOCAL, usage );
	}
};

/**
 * A HID Input Report.
 *
 * The input reports are generated on the DEVICE and are meant for
 * communication in the direction of DEVICE to HOST over the I2C transport.
 * When the DEVICE has active data it wishes to report to the HOST, it will
 * assert the Interrupt line associated with the HID protocol on the DEVICE.
 * When the HOST receives the Interrupt, it is responsible for reading the
 * data of the DEVICE via the Input Register (field: wInputRegister) as
 * defined in the HID Descriptor. The HOST does this by issuing an I2C read
 * request to the DEVICE.
 *
 * It is the responsibility of the DEVICE to assert the interrupt until all
 * the data has been read for that specific report. After reading the Input
 * Report, the DEVICE can continue to or reassert the interrupt if there are
 * additional Input Report(s) to be retrieved from the DEVICE.
 */
struct HIDInputReport
{
};

/**
 * A HID Output Report.
 *
 * The output report is generated on the HOST and is meant for communication
 * in the direction of HOST to DEVICE over the I2C transport. When the HOST
 * has active data it wishes to report to the DEVICE, it will write the output
 * report to the output register (`HidDescriptor::wOutputRegister`).
 */
struct HIDOutputReport
{
};

struct HIDFeatureReport
{
};

class Hid
{
};

/* End of file */
