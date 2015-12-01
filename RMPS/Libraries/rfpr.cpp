#include "rfpr.h"
#include "rfhw.h"
#include <stdint.h>
#include <string.h>

namespace rf {
	uint8_t crc8_update(uint8_t input, uint8_t lastCrc);

	void getByteArrayForConnectRequest(struct connectRequest data, uint8_t* bytearray) {
		bytearray[0] = ((uint8_t)(1 << 4));
		bytearray[1] = (uint8_t)(data.RID >> 8);
		bytearray[2] = ((uint8_t)(data.RID));
		
		uint8_t crc = 0;
		for (int i = 0; i < 3; i++) {
			crc = crc8_update(bytearray[i], crc);
		}
		bytearray[0] |= ((uint8_t)(crc & 0xf));
	}

	void getByteArrayForConnectConfirmation(struct connectedConfirmation data, uint8_t* bytearray) {
		bytearray[0] = ((uint8_t)(2 << 4)) | ((uint8_t)(data.VID & 0xf));
		bytearray[1] = ((uint8_t)(data.RID >> 8));
		bytearray[2] = ((uint8_t)(data.RID));
		
		uint8_t crc = 0;
		for (int i = 0; i < 3; i++) {
			crc = crc8_update(bytearray[i], crc);
		}
		bytearray[3] = ((uint8_t)(crc));
	}

	void getByteArrayForPing(struct ping data, uint8_t* bytearray) {
		bytearray[0] = ((uint8_t)(3 << 4)) | ((uint8_t)(data.VID & 0xf));
		
		uint8_t crc = 0;
		for (int i = 0; i < 1; i++) {
			crc = crc8_update(bytearray[i], crc);
		}
		bytearray[1] = ((uint8_t)(crc));
	}

	void getByteArrayForDatasending(struct dataSending data, uint8_t* bytearray) {
		bytearray[0] = ((uint8_t)(4 << 4)) | 4;
		
		memcpy(data.data, bytearray+1, 20 * sizeof(uint16_t));
		
		uint8_t* array = bytearray + 1;
		uint8_t crc = 0;
		for (int i = 0; i < 20 * 2; i+=2) {
			/*crc = crc8_update(array[i], 0);
			crc = crc8_update(array[i + 1], crc);*/
			
			// crc erstattet af dublikeret data
			
			// Indsæt crc
			// 10 bits til data
			// i = most significants 8 bits only 2 least significant bits used for data
			// j = least significant 8 bits using 4 bits since we need to use 
			
			// i i j j j j o o
			
			array[i] |= array[i] << 6 | ((array[i + 1] >> 2) & (0xf << 2));
		}
	}
	
	bool pr_send(connectRequest input) {
		uint8_t bytearray[3];
		getByteArrayForConnectRequest(input, bytearray);
		
		return hw_send(bytearray, 3);
	}
	
	bool pr_send(connectedConfirmation input) {
		uint8_t bytearray[4];
		getByteArrayForConnectConfirmation(input, bytearray);
		
		return hw_send(bytearray, 3);
	}
	
	bool pr_send(ping input) {
		uint8_t bytearray[2];
		getByteArrayForPing(input, bytearray);
	
		return hw_send(bytearray, 3);
	}
	
	bool pr_send(dataSending input) {
		uint8_t bytearray[21];
		getByteArrayForDatasending(input, bytearray);
		
		return hw_send(bytearray, 3);
	}
	
	bool pr_send_connectRequest(uint16_t RID) {
		struct connectRequest myConnectRequest;
		myConnectRequest.RID = RID;
		return pr_send(myConnectRequest);
	}
	
	bool pr_send_connectedConfirmation(uint16_t RID, uint8_t VID) {
		struct connectedConfirmation myConnectedConfirmation;
		myConnectedConfirmation.RID = RID; 
		myConnectedConfirmation.VID = VID;
		return pr_send(myConnectedConfirmation);
	}
	
	bool pr_send_ping(char VID) {
		struct ping myPing;
		myPing.VID = VID;
		return pr_send(myPing);
	}
	
	bool pr_send_dataSending(uint16_t data[]) {
		struct dataSending mydataSending;
		memcpy(data, mydataSending.data, 20 * sizeof(uint16_t));
		return pr_send(mydataSending);
	}
	
	packetTypes pr_receive(char* output){
		uint8_t lenght = 0;
		uint8_t* data = (uint8_t*)hw_recieve(&lenght);
		if (data == NULL)
			return NODATA;
		
		if(data[0] >> 4 == 1){
			struct connectRequest myConnectRequest;
			myConnectRequest.RID = data[1];
			myConnectRequest.RID = myConnectRequest.RID << 8 | data[2];
			
			uint8_t crc = 0;
			for (int i = 0; i < 3; i++) {
				uint8_t d = data[i];
				
				// Data of index 0 contains 4 bit packet type and 4 bit checksum
				// Only validate packet type
				if (i == 0) {
					d = d & 0xf0;
				}
				
				crc = crc8_update(d, crc);
			}
			
			if ((crc & 0xf) == (data[0] & 0xf)){
				// Update output
				memcpy(output, &myConnectRequest, sizeof(connectRequest));
			
				return CONNECT_REQUEST;
			}
			
		}else if(data[0] >> 4 == 2){
			struct connectedConfirmation myConnectedConfirmation;
			myConnectedConfirmation.VID = data[1] >> 8; 
			myConnectedConfirmation.RID = data[2] << 8 | data[3]; 
			//*output = myConnectedConfirmation;
			
			uint8_t crc = 0;
			for (int i = 0; i < 3; i++) {
				crc = crc8_update(data[i], crc);
			}
			
			if(crc == data[3]){
				return CONNECTED_CONFIRMATION;
			}
		}else if(data[0] >> 4 == 3){
			struct ping myPing;
			myPing.VID = data[0] >> 8; 
			//*output = (void*)myPing;
			
			uint8_t crc = 0;
			crc = crc8_update(data[0], crc);
			
			if(crc == data[1]){
				return PING;
			}
		}else if(data[0] >> 4 == 4){
			struct dataRecieving dataRecieving;
			
			uint8_t* array = data + 1;
			for (int i = 0; i < 20; i++) {
				dataRecieving.data[i].value = array[i * 2] & 0x3 | array[i * 2 + 1];
				dataRecieving.data[i].valid = (((array[i * 2] & 0x3) << 6) | array[i * 2 + 1] >> 2) == array[i*2] & 0xfc;
			}
			
			return DATA;
		}
		
		return INVALID;
	}
	
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