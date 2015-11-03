namespace rf {
	struct connectRequest {
		uint16_t RID,
		byte Typebit,
		byte Check
	};

	struct connectedConfirmation {
		byte VID,
		uint16_t RID,
		byte Typebit,
		byte checksum
	};

	struct ping {
		byte Type,
		byte VID,
		byte checksum
	};

	struct dataSending {
		byte Type,
		uint16_t Data[20],
		uint16_t checksum
	};

	byte* getByteArrayForConnectRequest(struct connectRequest) {
		byte bytearray[3];

		bytearray[0] = ((uint8_t)(connectRequest.RID >> 8));
		bytearray[1] = ((uint8_t)(connectRequest.RID));
		bytearray[2] = ((uint8_t)(connectRequest.Typebit << 4)) | ((uint8_t)(connectRequest.Check << 4));

		return bytearray;
	}

	void pr_send();
	void pr_receive()
}