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

// Check PS/2 object times out
DEFINE_TEST(ps2_timeout)
{
	Ps2 ps2(0, 1);
	ps2.m_state = Ps2State::ReadingWord;
	unsigned long target;
	// Get a value that doesn't wrap
	do
	{
		target = micros() + 1000;
	} while (target < micros());

	do
	{
		ps2.poll();
	}
	while(micros() < target);
	return (ps2.m_state == Ps2State::Idle);
}

// Check PS/2 can collect bits
DEFINE_TEST(ps2_collect_bits)
{
	// 0 is clk, 1 = data
	Ps2 ps2(0, 1);
	uint16_t test_word = (0x03 << 9) | (0xAA << 1);
	for (int i = 0; i < 11; i++)
	{
		pin_results[0] = 1;
		ps2.poll();
		ps2.poll();
		ps2.poll();
		ps2.poll();
		pin_results[1] = (test_word & (1 << i)) ? 1 : 0;
		pin_results[0] = 0;
		ps2.poll();
		ps2.poll();
		ps2.poll();
		ps2.poll();
	}

	int read_byte = ps2.readBuffer();
	// Should have collected 0xAA
	return (read_byte == 0xAA);
}

DEFINE_TEST(ps2_validate_words)
{
	const int inputs[] = {0x600, 0x606, 0x402};
	const int outputs[] = {0x00, 0x03, 0x01};
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

void digitalWrite(int pin, bool level)
{
#ifdef VERBOSE_DEBUG
	printf("Writing pin %d = %d\r\n", pin, level);
#endif
}

void pinMode(int pin, int mode)
{
#ifdef VERBOSE_DEBUG
	printf("Setting pin %d mode = %d\r\n", pin, mode);
#endif
}

int digitalRead(int pin)
{
#ifdef VERBOSE_DEBUG
	printf("Reading pin %d = %d\r\n", pin, pin_results[pin]);
#endif
	return pin_results[pin];
}

unsigned long micros() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	unsigned long time = now.tv_sec * 1000000;
	time += now.tv_nsec / 1000;
	return time;
}

const test_fn_t TESTS[] = {
	ps2_timeout,
	ps2_collect_bits,
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
