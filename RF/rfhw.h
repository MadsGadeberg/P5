namespace rf {
	void hw_init(uint8_t byte_filter);
	bool hw_send(const char buffer[], uint8_t len);
	bool hw_canSend();
	char* hw_recieve(uint8_t* length);
}