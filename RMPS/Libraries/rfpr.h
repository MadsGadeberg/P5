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

	// This struct contains the all samples in a ping sequence.
	struct sampleDataPacket {
		uint16_t data[20];
	};

	// struct that contains data about one sample and a bit that tels if the data is verified to be valid.
	struct sampleData {
		bool valid;
		uint16_t value;
	};

	// this struct contains all samples on a ping sequence. Exacly like sampleDataPacket except that this data is verified if the fifferent samples have an error validating bit after transmitting. 
	struct sampleDataPacketVerified {

		sampleData data[20];
	};
	
	enum packetTypes {
		CONNECT_REQUEST, CONNECTED_CONFIRMATION, PING, DATA, INVALID, NODATA
	};
	typedef enum packetTypes packetTypes;
	
	bool pr_send(connectRequest input);
	bool pr_send(connectedConfirmation input);
	bool pr_send(ping input);
	bool pr_send(sampleDataPacket input);
	
	bool pr_send_connectRequest(uint16_t RID);
	bool pr_send_connectedConfirmation(uint16_t RID, uint8_t VID);
	bool pr_send_ping(char VID);
	bool pr_send_sampleDataPacket(uint16_t data[]);
	
	packetTypes pr_receive(char* output);
}