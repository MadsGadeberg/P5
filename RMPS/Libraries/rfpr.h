#include <stdint.h>
#include "rfapp.h"

namespace rf {
	struct ConnectRequest {
		uint16_t RID;
	};

	struct ConnectedConfirmation {
		uint8_t VID;
		uint16_t RID;
	};

	struct Ping {
		uint8_t VID;
	};

	// This struct contains the all samples in a ping sequence.
	struct SamplePacket {
		uint16_t* data;
	};

	// struct that contains data about one sample and a bit that tels if the data is verified to be valid.
	struct Sample {
		bool valid;
		uint16_t value;
	};

	// this struct contains all samples on a ping sequence. Exacly like samplePacket except that this data is verified if the fifferent samples have an error validating bit after transmitting. 
	struct SamplePacketVerified {
		Sample data[SAMPLE_PACKET_SIZE];
	};

	enum PacketTypes {
		CONNECT_REQUEST, CONNECTED_CONFIRMATION, PING, DATA, INVALID, NODATA
	};
	typedef enum PacketTypes packetTypes;

	bool pr_send(ConnectRequest input);
	bool pr_send(ConnectedConfirmation input);
	bool pr_send(Ping input);
	bool pr_send(SamplePacket input);
	
	bool pr_send_connectRequest(uint16_t RID);
	bool pr_send_connectedConfirmation(uint16_t RID, uint8_t VID);
	bool pr_send_ping(char VID);
	bool pr_send_samplePacket(uint16_t data[]);
	
	PacketTypes pr_receive(char* output);
	PacketTypes pr_receive(char* output, uint8_t* data, uint8_t len);
}