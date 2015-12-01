#include <stdint.h>

namespace rf {
	void hw_init(uint8_t byte_filter);
	bool hw_send(uint8_t byte);
	bool hw_send(const uint8_t buffer[], uint8_t len);
	volatile uint8_t* hw_recieve(uint8_t* length);
}