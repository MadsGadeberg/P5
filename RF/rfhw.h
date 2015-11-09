namespace rf {
	void hw_initSPI();
	void hw_init(uint8_t byte_filter);
	void hw_interrupt();
	void hw_enableRF();
	void hw_disableRF();
	void hw_setStateRecieve();
	void hw_setStateIdle();
	void hw_setStateTransmitter();
	void hw_setStateSleep();
	bool hw_send(char byte);
	bool hw_send(const char buffer[], uint8_t len);
	char* hw_recieve(uint8_t* length);
	uint16_t hw_sendCMD(uint16_t command);
	char hw_sendCMDByte(char out);
}