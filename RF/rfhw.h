#include <stdint.h>

namespace rf {
	void hw_init(uint8_t byte_filter);
	bool hw_send(char byte);
	bool hw_send(const char buffer[], uint8_t len);
	char* hw_recieve(uint8_t* length);
}