#include "rfpr.h"
#include "rfhw.h"
#include <stdint.h>

namespace rf {
	byte* getByteArrayForConnectRequest(struct connectRequest) {
		byte bytearray[3];
		bytearray[0] = ((uint8_t)(connectRequest.RID >> 8));
		bytearray[1] = ((uint8_t)(connectRequest.RID));
		bytearray[2] = ((uint8_t)(1 << 4)) | ((uint8_t)(connectRequest.checksum << 4));
		return bytearray;
	}

	byte* getByteArrayForConnectConfirmation(struct connectedConfirmation) {
		byte bytearray[4];
		bytearray[0] = ((uint8_t)(connectedConfirmation.VID << 4)) | ((uint8_t)(2 << 4));
		bytearray[1] = ((uint8_t)(connectRequest.RID >> 8));
		bytearray[2] = ((uint8_t)(connectRequest.RID));
		bytearray[3] = ((uint8_t)(connectRequest.Checksum));
		return bytearray;
	}

	byte* getByteArrayForPing(struct ping) {
		byte bytearray[2];
		bytearray[0] = ((uint8_t)(3 << 4)) | ((uint8_t)(ping.VID << 4));
		bytearray[1] = ((uint8_t)(ping.Checksum));
		return bytearray;
	}

	byte* getByteArrayForDatasending(struct dataSending) {
		byte bytearray[2];
		bytearray[0] = ((uint8_t)(4 << 4));
		memcpy(dataSending.Data, bytearray+1, 20 * sizeof(uint16_t));
		return bytearray;
	}


	pr_send(){

	}

	pr_receive(){

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