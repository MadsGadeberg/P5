#include "rfpr.h"
#include "rfhw.h"
#include <stdint.h>
#include <string.h>

namespace rf {
	char* getByteArrayForConnectRequest(struct connectRequest data) {
		static char bytearray[3];
		bytearray[0] = ((uint8_t)(data.RID >> 8));
		bytearray[1] = ((uint8_t)(data.RID));
		bytearray[2] = ((uint8_t)(1 << 4)) | ((uint8_t)(data.checksum << 4));
		return bytearray;
	}

	char* getByteArrayForConnectConfirmation(struct connectedConfirmation data) {
		static char bytearray[4];
		bytearray[0] = ((uint8_t)(data.VID << 4)) | ((uint8_t)(2 << 4));
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
				struct connectRequest myPacket;
				myPacket.RID = RID;
				hw_send(getByteArrayForConnectRequest(myPacket), 3);
				break;
			case connectedConfirmationPacket:
				struct connectConfirmation myPacket;
				myPacket.RID = RID; myPacket.VID = VID;
				hw_send(getByteArrayForConnectConfirmation(myPacket), 4);
				break; 
			case pingPacket:
				struct ping myPacket;
				myPacket.VID = VID;
				hw_send(getByteArrayForPing(myPacket), 2);
				break;
			case dataSendingPacket:
				break;
		}
		return true;
	}

	bool pr_receive(){
		return true;
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