#include <stdint.h>

namespace rf {
	struct connectRequest {
		uint16_t RID;
		uint16_t checksum;
	};

	struct connectedConfirmation {
		char VID;
		uint16_t RID;
		uint32_t checksum;
	};

	struct ping {
		char VID;
		uint8_t checksum;
	};

	struct dataSending {
		uint16_t data[20];
		uint16_t checksum;
	};

	enum packetTypes {
		connectRequestPacket, connectedConfirmationPacket, pingPacket, dataSendingPacket
	};
	typedef enum packetTypes packetTypes;

	bool pr_send(packetTypes packetType, uint16_t RID, char VID, uint16_t data);
	bool pr_receive(void* output);
	bool pr_send_connectRequest(uint16_t RID, char VID);
}