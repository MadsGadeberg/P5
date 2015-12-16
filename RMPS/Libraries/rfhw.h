#include <stdint.h>

namespace rf {
	void phy_init(uint8_t byte_filter);
	bool phy_send(uint8_t byte);
	bool phy_send(const uint8_t buffer[], uint8_t len);
	bool phy_sendWait(const uint8_t buffer[], uint8_t len);
	uint8_t* phy_receive(uint8_t* length);
}