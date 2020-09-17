/**
 * Runs some unit tests.
 */

#define TEST_MODE_NO_PRIVATE

#include <cstdio>
#include <cstdint>
#include <ctime>

#include "arduino_stubs.h"

#include "ps2.h"

#define MAX_PINS 4
int pin_results[MAX_PINS];

static void testClockInput( void )
{
  // ?
}

static void testClockOutput(Ps2::Level level)
{
  // ?
}

static void testDataInput( void )
{
  // ?
}

static void testDataOutput(Ps2::Level level)
{
  // ?
}


typedef bool (*test_fn_t)(const char **sz_name);

#define DEFINE_TEST(test_name)                  \
	static bool test_name##_inner(void);        \
	static bool test_name(const char **sz_name) \
	{                                           \
		if (sz_name)                            \
		{                                       \
			*sz_name = __func__;                \
		}                                       \
		return test_name##_inner();             \
	}                                           \
	static bool test_name##_inner(void)

// Check PS/2 can collect bits
DEFINE_TEST(ps2_collect_bits)
{
	// 0 is clk, 1 = data
	Ps2 ps2(testClockInput, testClockOutput, testDataInput, testDataOutput);
	uint16_t test_word = (0x03 << 9) | (0xAA << 1) | 0;

	for (int i = 0; i < 12; i++)
	{
		ps2.clockEdge( Ps2::Edge::EDGE_RISING, Ps2::Level::LEVEL_LOW );
		ps2.clockEdge( Ps2::Edge::EDGE_FALLING, (test_word & (1 << i)) ? Ps2::Level::LEVEL_HIGH : Ps2::Level::LEVEL_LOW );
	}

	int read_byte = ps2.poll();
	// Should have collected 0xAA
	return (read_byte == 0xAA);
}

// Check PS/2 can handle a timeout when collecting bits
DEFINE_TEST(ps2_collect_bits_timeout)
{
	// 0 is clk, 1 = data
	Ps2 ps2(testClockInput, testClockOutput, testDataInput, testDataOutput);

	uint16_t test_word = (0x03 << 9) | (0xEE << 1) | 0;
	// Send some of the bits
	for (int i = 0; i < 5; i++)
	{
		ps2.clockEdge( Ps2::Edge::EDGE_RISING, Ps2::Level::LEVEL_LOW );
		ps2.clockEdge( Ps2::Edge::EDGE_FALLING, (test_word & (1 << i)) ? Ps2::Level::LEVEL_HIGH : Ps2::Level::LEVEL_LOW );
	}
	// Oops .. didn't collect enough! This should now flush out the partial word.
	for( int i = 0; i < 1000; i++ )
	{
		ps2.poll();
	}

	// Send a different word
	test_word = (0x03 << 9) | (0xD1 << 1) | 0;
	for (int i = 0; i < 16; i++)
	{
		ps2.clockEdge( Ps2::Edge::EDGE_RISING, Ps2::Level::LEVEL_LOW );
		ps2.clockEdge( Ps2::Edge::EDGE_FALLING, (test_word & (1 << i)) ? Ps2::Level::LEVEL_HIGH : Ps2::Level::LEVEL_LOW );
	}

	int read_byte = ps2.poll();
	// Should have collected 0xD1
	return (read_byte == 0xD1);
}

// Check PS/2 can verify valid words
DEFINE_TEST(ps2_validate_words)
{
	const int inputs[] = {0x600, 0x606, 0x402, 0x401};
	const int outputs[] = {0x00, 0x03, 0x01, -1};
	bool pass = true;
	for (int i = 0; i < 3; i++)
	{
		int result = Ps2::validateWord(inputs[i]);
		pass &= (result == outputs[i]);
	}
	return pass;
}

DEFINE_TEST(ps2_encode_bytes)
{
	bool pass = true;
	for(int i = 0; i < 256; i++)
	{
		uint16_t word = Ps2::encodeByte((uint8_t) i);
		uint8_t output = Ps2::validateWord(word);
		if (i != output) {
			printf("%02x != %02x (%04x)\n", i, output, word);
			pass = false;
		}
	}
	return pass;
}

int bitRead(int word, int bit)
{
	return ((word & (1 << bit)) != 0) ? 1 : 0;
}

const test_fn_t TESTS[] = {
	ps2_collect_bits,
	ps2_collect_bits_timeout,
	ps2_validate_words,
	ps2_encode_bytes,
};

int main(int argc, char **argv)
{
	for (size_t i = 0; i < sizeof(TESTS) / sizeof(TESTS[0]); i++)
	{
		const char *sz_name = NULL;
		bool result = TESTS[i](&sz_name);
		printf("Test %-20s: %s\r\n", sz_name, result ? "PASS" : "FAIL");
	}
}
