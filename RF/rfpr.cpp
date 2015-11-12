#include "rfpr.h"
#include "rfhw.h"
#include <stdint.h>
#include <string.h>
#include <type_traits>

namespace rf {
	char* getByteArrayForConnectRequest(struct connectRequest data) {
		static char bytearray[3];
		bytearray[0] = ((uint8_t)(1 << 4)) | ((uint8_t)(data.checksum << 4));
		bytearray[1] = ((uint8_t)(data.RID >> 8));
		bytearray[2] = ((uint8_t)(data.RID));
		return bytearray;
	}

	char* getByteArrayForConnectConfirmation(struct connectedConfirmation data) {
		static char bytearray[4];
		bytearray[0] = ((uint8_t)(2 << 4)) | ((uint8_t)(data.VID << 4));
		bytearray[1] = ((uint8_t)(data.RID >> 8));
		bytearray[2] = ((uint8_t)(data.RID));
		bytearray[3] = ((uint8_t)(data.checksum));
		return bytearray;
	}

	char* getByteArrayForPing(struct ping data) {
		static char bytearray[2];
		bytearray[0] = ((uint8_t)(3 << 4)) | ((uint8_t)(data.VID << 4));
		bytearray[1] = ((uint8_t)(data.checksum));
		return bytearray;
	}

	char* getByteArrayForDatasending(struct dataSending data) {
		static char bytearray[2];
		bytearray[0] = ((uint8_t)(4 << 4));
		memcpy(data.data, bytearray+1, 20 * sizeof(uint16_t));
		return bytearray;
	}

	bool pr_send(packetTypes packetType, uint16_t RID, char VID, uint16_t data){
		switch (packetType)
		{
			case connectRequestPacket:
				struct connectRequest myConnectRequest;
				myConnectRequest.RID = RID;
				hw_send(getByteArrayForConnectRequest(myConnectRequest), 3);
				break;
			case connectedConfirmationPacket:
				struct connectedConfirmation myConnectedConfirmation;
				myConnectedConfirmation.RID = RID; 
				myConnectedConfirmation.VID = VID;
				hw_send(getByteArrayForConnectConfirmation(myConnectedConfirmation), 4);
				break; 
			case pingPacket:
				struct ping myPing;
				myPing.VID = VID;
				hw_send(getByteArrayForPing(myPing), 2);
				break;
			case dataSendingPacket:
				//struct datasending mydatasending;
				break;
		}
		return true;
	}
	
	bool pr_send_connectRequest(uint16_t RID, char VID) {
		struct connectRequest myConnectRequest;
		myConnectRequest.RID = RID;
		return pr_send(myConnectRequest);
	}
	
	bool pr_send_connectedConfirmation(uint16_t RID) {
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
	
	bool pr_send_dataSending(uint16_t data) {
		struct dataSending mydataSending;
		mydataSending.data = data;
		return pr_send(mydataSending);
	}
	
	
	
	
	
	bool pr_send(struct connectRequest input) {
		return hw_send(getByteArrayForConnectRequest(input), 3);
	}
	
	bool pr_send(struct connectedConfirmationPacket input) {
		return hw_send(getByteArrayForConnectConfirmation(input), 3);
	}
	
	bool pr_send(struct Ping input) {
		return hw_send(getByteArrayForPing(input), 3);
	}
	
	bool pr_send(struct dataSendingPacket input) {
		return hw_send(getByteArrayForDatasending(input), 3);
	}
	
	packetTypes pr_receive(void* output){
		uint8_t lenght = 0;
		char* data = hw_recieve(&lenght);
		
		if(data[0] >> 4 == 1){
			struct connectRequest myConnectRequest;
			myConnectRequest.RID = data[1] << 8 | data[2];
			*ouput = myConnectRequest;
			return connectRequestPacket;
			
		}else if(data[0] >> 4 == 2){
			struct connectedConfirmation myConnectedConfirmation;
			myConnectedConfirmation.VID = data[1] >> 8; 
			myConnectedConfirmation.RID = data[2] << 8 | data[3]; 
			*output = myConnectedConfirmation;
			return myConnectedConfirmation;
		}else if(data[0] >> 4 == 3){
			struct ping myPing;
			myPing.VID = data[0] >> 8; 
			*output = myPing;
			return myPing;
		}else if(data[0] >> 4 == 4){
			
			
		}
		
		return 0;
	}

	uint8_t crc16_update(uint8_t a)
	{
		uint8_t crc = 0;
		int i;
		crc ^= a;
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