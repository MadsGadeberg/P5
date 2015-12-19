#include "rfpr.h"
#include "rfphy.h"
#include "rfapp.h"
#include <stdint.h>
#include <string.h>

namespace rf {
	uint8_t crc8_update(uint8_t input, uint8_t lastCrc);

	// Get a byte array from struct connectRequest
	void getByteArrayForConnectRequest(struct ConnectRequest data, uint8_t* bytearray) {
		// First byte contains of 4 bit packet type and 4 bit crc
		bytearray[0] = ((uint8_t)(1 << 4));
		
		// Transfer the uint16_t RID to array
		bytearray[1] = (uint8_t)(data.RID >> 8);
		bytearray[2] = ((uint8_t)(data.RID));
		
		// Calculate 8 bit checksum
		uint8_t crc = 0;
		for (int i = 0; i < 3; i++) {
			crc = crc8_update(bytearray[i], crc);
		}
		
		// Set checksum (bitwise and to only use 4 bits)
		bytearray[0] |= ((uint8_t)(crc & 0xf));
	}

	// Get a byte array for struct connectedConfirmation
	void getByteArrayForConnectConfirmation(ConnectedConfirmation data, uint8_t* bytearray) {
		// The most significant bits of index 0 contains the packet type
		bytearray[0] = ((uint8_t)(2 << 4));
		
		// The least significant 4 bits is set to VID
		bytearray[0] |= ((uint8_t)(data.VID & 0xf));
		
		// Transfer the uint16_t RID to array
		bytearray[1] = ((uint8_t)(data.RID >> 8));
		bytearray[2] = ((uint8_t)(data.RID));
		
		// Calculate 8 bit checksum
		uint8_t crc = 0;
		for (int i = 0; i < 3; i++) {
			crc = crc8_update(bytearray[i], crc);
		}
		bytearray[3] = ((uint8_t)(crc));
	}

	// Get a byte array for struct ping
	void getByteArrayForPing(struct Ping data, uint8_t* bytearray) {
		// The most significant bits of index 0 contains the packet type
		bytearray[0] = ((uint8_t)(3 << 4));
		
		// The least significant 4 bits is set to VID
		bytearray[0] |= ((uint8_t)(data.VID & 0xf));
		
		// Calculate 8 bit checksum
		uint8_t crc = 0;
		for (int i = 0; i < 1; i++) {
			crc = crc8_update(bytearray[i], crc);
		}
		bytearray[1] = ((uint8_t)(crc));
	}

	// Get a byte array for struct samplePacket
	void getByteArrayForsamplePacket(struct SamplePacket data, uint8_t* bytearray) {
		// Both the most significant and the least significant 4 bits contains the packet type
		bytearray[0] = ((uint8_t)(4 << 4)) | 4;
		
		// Starts at index 1
		uint8_t* array = bytearray + 1;
		
		// Copy data from samplePacket + 1 because we need to start at index 1 course index 0 is used for packet type
		for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
			array[i * 2] = (data.data[i] >> 8);
			array[i * 2 + 1] = (data.data[i]);
		
			// crc replaced by dublicated data
			// This means that the error will be max the 4 least significant bits with is an error of max 15 (all 4 bits switched from 0 to 1).
			// The error is within the accetable margin
			
			// 10 bits for data
			
			// 16 bits space
			// i = most significants 8 bits and using the 2 least significant bits since it's a 10 bit data
			// j = least significant 8 bits using the 4 most significant bits
			// i i j j j j o o

			// Sets 2 most significant to the 2 most significant bits of the 10 bit data
			uint8_t check = (array[i * 2] << 6);
			
			// Sets the 4 bits in the middle
			check |= (array[i * 2 + 1] >> 2) & 0x2C;
			
			// Update array
			array[i * 2] |= check;
		}
	}

	// Send a connectRequest
	bool pr_send(ConnectRequest input) {
		uint8_t bytearray[3];
		getByteArrayForConnectRequest(input, bytearray);
		
		return phy_send(bytearray, 3);
	}
	
	// Sends a connectConfirmation
	bool pr_send(ConnectedConfirmation input) {
		uint8_t bytearray[4];
		getByteArrayForConnectConfirmation(input, bytearray);
		
		return phy_send(bytearray, 4);
	}
	
	// Sends a ping
	bool pr_send(Ping input) {
		uint8_t bytearray[2];
		getByteArrayForPing(input, bytearray);
	
		return phy_send(bytearray, 2);
	}
	
	// Sends a samplePacket
	bool pr_send(SamplePacket input) {
		uint8_t bytearray[SAMPLE_PACKET_SIZE];
		getByteArrayForsamplePacket(input, bytearray);
		
		return phy_send(bytearray, SAMPLE_PACKET_SIZE);
	}
	
	// Sends a connect request with the specified RID
	bool pr_send_connectRequest(uint16_t RID) {
		struct ConnectRequest request;
		request.RID = RID;
		return pr_send(request);
	}
	
	// Sends a connectConfirmation with the specified RID and VID
	bool pr_send_connectedConfirmation(uint16_t RID, uint8_t VID) {
		struct ConnectedConfirmation confirmation;
		confirmation.RID = RID; 
		confirmation.VID = VID;
		return pr_send(confirmation);
	}
	
	// Sends a ping with the specified VID
	bool pr_send_ping(char VID) {
		struct Ping ping;
		ping.VID = VID;
		return pr_send(ping);
	}
	
	// Sends a samplePacket with the specified data
	bool pr_send_samplePacket(uint16_t data[]) {
		struct SamplePacket samples;
		samples.data = data;
		return pr_send(samples);
	}
	
	PacketTypes pr_receive(char* output) {
		// Read from hardware layer and check if any data is received
		uint8_t len = 0;
		uint8_t* data = (uint8_t*)phy_receive(&len);
		if (data == NULL)
			return NODATA;
			
		return pr_receive(output, data, len);
	}
	
	// receive data. Outputs the packet type as return and the struct with the output parameter output
	// Reads from data instead of rf_receive
	PacketTypes pr_receive(char* output, uint8_t* data, uint8_t len) {
		if (data == NULL)
			return NODATA;
		
		// Packettype 1 is connectRequest
		if (data[0] >> 4 == 1) {
			struct ConnectRequest request;
			
			// Read RID with is in index 1 and 2 (uint16_t)
			request.RID = data[1];
			request.RID = request.RID << 8 | data[2];
			
			// Calculate crc
			uint8_t crc = 0;
			for (int i = 0; i < 3; i++) {
				uint8_t dataToValidate = data[i];
				
				// Data of index 0 contains 4 bit packet type and 4 bit checksum
				// Only validate packet type
				if (i == 0) {
					dataToValidate &= 0xf0;
				}
				
				crc = crc8_update(dataToValidate, crc);
			}
			
			// Check if checksum matches
			if ((crc & 0xf) == (data[0] & 0xf)) {
				// Update output
				memcpy(output, &request, sizeof(ConnectRequest));
			
				return CONNECT_REQUEST;
			}
		} else if (data[0] >> 4 == 2) {
			struct ConnectedConfirmation confirmation;
			
			// Read VID
			confirmation.VID = data[0] & 0xf;
			
			// Read RID with is in index 1 and 2 (uint16_t)
			confirmation.RID = data[1];
			confirmation.RID = confirmation.RID << 8 | data[2];
			
			// Calculate crc
			uint8_t crc = 0;
			for (int i = 0; i < 3; i++) {
				crc = crc8_update(data[i], crc);
			}
			
			// Check if checksum matches
			if (crc == data[3]) {
				// Update output
				memcpy(output, &confirmation, sizeof(ConnectedConfirmation));
			
				return CONNECTED_CONFIRMATION;
			}
		} else if (data[0] >> 4 == 3) {
			struct Ping ping;
			
			// Read VID
			ping.VID = data[0] & 0xf;
			
			uint8_t crc = 0;
			crc = crc8_update(data[0], crc);
			
			if (crc == data[1]) {
				// Update output
				memcpy(output, &ping, sizeof(Ping));
			
				return PING;
			}
		} else if (data[0] >> 4 == 4 || data[0] & 0xf == 4) { // This else needs to be at last so it does not detect other package types
			struct SamplePacketVerified samplePacketVerified;
			
			uint8_t* array = data + 1;
			for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
				// Sets 2 most significant to the 2 most significant bits of the 10 bit data
				uint8_t check = (array[i * 2] << 6);
			
				// Sets the 4 bits in the middle
				check |= (array[i * 2 + 1] >> 2) & 0x2C;
			
				samplePacketVerified.data[i].value = (uint16_t) ((array[i * 2] & 0x3) << 8) | (uint16_t) array[i * 2 + 1];
				samplePacketVerified.data[i].valid = check == (array[i*2] & 0xfc);
			}
			
			// Update output
			memcpy(output, &samplePacketVerified, sizeof(SamplePacketVerified));
			
			return DATA;
		}
		
		return INVALID;
	}
	
	// http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
	uint8_t crc8_update(uint8_t input, uint8_t lastCrc)
	{
		uint8_t crc = lastCrc;
		int i;
		crc ^= input;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;
			else
				crc = (crc >> 1);
		}
		return crc;
	}
}
