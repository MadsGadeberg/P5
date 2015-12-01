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
		CONNECT_REQUEST, CONNECTED_CONFIRMATION, PING, DATA, INVALID, NODATA
	};
	typedef enum packetTypes packetTypes;

	bool pr_send(connectRequest input);
	bool pr_send(connectedConfirmation input);
	bool pr_send(ping input);
	bool pr_send(dataSending input);

	bool pr_send_connectRequest(uint16_t RID);
	bool pr_send_connectedConfirmation(uint16_t RID, uint8_t VID);
	bool pr_send_ping(char VID);
	bool pr_send_dataSending(uint16_t data[]);

	packetTypes pr_receive(char* output);
}