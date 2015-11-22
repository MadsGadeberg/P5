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
	
	struct packetdata {
		bool valid;
		uint16_t value;
    };
	
	struct dataRecieving {
		packetdata data[20];
	};
	
	enum packetTypes {
		CONNECT_REQUEST, CONNECTED_CONFIRMATION, PING, DATA, INVALID
	};
	typedef enum packetTypes packetTypes;

	char* pr_receive();
}