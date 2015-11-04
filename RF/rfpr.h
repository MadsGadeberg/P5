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
		uint16_t Data[20]
	};

	void pr_send();
	void pr_receive()
}