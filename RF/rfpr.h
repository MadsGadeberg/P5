#include <stdint.h>

namespace rf {
	struct connectRequest {
		uint16_t RID;
	};

	struct connectedConfirmation {
		char VID;
		uint16_t RID;
	};

	struct ping {
		char VID;
	};

	struct dataSending {
		uint16_t data[20];
	};
	
	struct dataRecieving {
		packetdata data[20];
	};
	
	struct packetdata {
		bool valid;
		uint16_t value;
	}

	enum packetTypes {
		connectRequestPacket, connectedConfirmationPacket, pingPacket, dataSendingPacket
	};
	typedef enum packetTypes packetTypes;

	bool pr_send(packetTypes packetType, uint16_t RID, char VID, uint16_t data);
	char* pr_receive();
}