#define INPUT_PULLUP 1
#define OUTPUT 0
#define HIGH 1
#define LOW 0

int bitRead(int word, int bit);
void digitalWrite(int pin, bool level);
void pinMode(int pin, int mode);
int digitalRead(int pin);
unsigned long micros();