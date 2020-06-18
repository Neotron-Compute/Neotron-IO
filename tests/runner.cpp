/**
 * Runs some unit tests.
 */

#define TEST_MODE_NO_PRIVATE

#define INPUT_PULLUP 1
#define OUTPUT 0
#define HIGH 1
#define LOW 0
#define MAX_PINS 4

static int pin_results[MAX_PINS];

static void digitalWrite(int pin, bool level);
static void pinMode(int pin, int mode);
static int digitalRead(int pin);

#include <cstdio>
#include <cstdint>
#include "ps2.h"

typedef bool (*test_fn_t)(const char** sz_name);

static bool test1(const char** sz_name);
static bool test2(const char** sz_name);
static bool test3(const char** sz_name);

const test_fn_t TESTS[] = {
	test1,
	test2,
	test3,
};

int main(int argc, char** argv) {
	for(size_t i = 0; i < sizeof(TESTS) / sizeof(TESTS[0]); i++) {
		const char* sz_name = NULL;
		bool result = TESTS[i](&sz_name);
		printf("Test %20s: %s\r\n", sz_name, result ? "PASS" : "FAIL");
	}
}

// Check PS/2 object times out
static bool test1(const char** sz_name) {
	if (sz_name) {
		*sz_name = __func__;
	}
	Ps2 ps2(0, 1);
	ps2.m_state = State::Active;
	for(int i = 0; i < 1000; i++)
	{
		ps2.poll();
	}
	return (ps2.m_state == State::Idle);
}


// Check PS/2 can collect bits
static bool test2(const char** sz_name) {
	if (sz_name) {
		*sz_name = __func__;
	}
	// 0 is clk, 1 = data
	Ps2 ps2(0, 1);
	uint16_t test_word = (0x03 << 9) | (0xAA << 1);
	for(int i = 0; i < 11; i++) {
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

	int read_byte = ps2.read_buffer();
	// Should have collected 0xAA
	return (read_byte == 0xAA);
}

static bool test3(const char** sz_name) {
	if (sz_name) {
		*sz_name = __func__;
	}
	const int inputs[] = { 0x600, 0x606, 0x402 };
	const int outputs[] = { 0x00, 0x03, 0x01 };
	bool pass = true;
	for(int i = 0; i < 3; i++)
	{
		int result = Ps2::validate_word(inputs[i]);
		pass &= (result == outputs[i]);
	}
	return pass;
}

static void digitalWrite(int pin, bool level) {
#ifdef VERBOSE_DEBUG
	printf("Writing pin %d = %d\r\n", pin, level);
#endif
}

static void pinMode(int pin, int mode) {
#ifdef VERBOSE_DEBUG
	printf("Setting pin %d mode = %d\r\n", pin, mode);
#endif
}

static int digitalRead(int pin) {
#ifdef VERBOSE_DEBUG
	printf("Reading pin %d = %d\r\n", pin, pin_results[pin]);
#endif
	return pin_results[pin];
}
