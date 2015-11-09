namespace rf {
	struct connectRequest {
		uint16_t RID
	};

	struct connectedConfirmation {
		byte VID,
		uint16_t RID
	};

	struct ping {
		byte VID
	};

	struct dataSending {
		uint16_t data[20]
	};

	enum packetTypes {
		connectRequest, connectedConfirmation, ping, dataSending
	};
	typedef enum packetTypes packetTypes;

	bool pr_send(packetTypes packetType, uint16_t RID, byte VID, uint16_t data);
	bool pr_receive()
}