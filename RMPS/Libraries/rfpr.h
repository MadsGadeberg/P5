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
	struct samplePacket {
		uint16_t data[20];
	};

	// struct that contains data about one sample and a bit that tels if the data is verified to be valid.
	struct sample {
		bool valid;
		uint16_t value;
	};

	// this struct contains all samples on a ping sequence. Exacly like samplePacket except that this data is verified if the fifferent samples have an error validating bit after transmitting. 
	struct samplePacketVerified {
		sample data[20];
	};

	enum packetTypes {
		CONNECT_REQUEST, CONNECTED_CONFIRMATION, PING, DATA, INVALID, NODATA
	};
	typedef enum packetTypes packetTypes;
	
	void pr_initRF();

	bool pr_send(connectRequest input);
	bool pr_send(connectedConfirmation input);
	bool pr_send(ping input);
	bool pr_send(samplePacket input);
	
	bool pr_send_connectRequest(uint16_t RID);
	bool pr_send_connectedConfirmation(uint16_t RID, uint8_t VID);
	bool pr_send_ping(char VID);
	bool pr_send_samplePacket(uint16_t data[]);
	
	packetTypes pr_receive(char* output);
}