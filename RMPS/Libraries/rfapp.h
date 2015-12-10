// Global constants
#define GROUP 20
#define TIME_BETWEEN_SAMPLES 5	//
#define TIME_BETWEEN_PING 20	// Time between each satellite ping
#define TIME_BETWEEN_PING_SEQUENCE 200 // Time between each time we will ping all sattelites connected

#define SAMPLES_BETWEEN_PINGS TIME_BETWEEN_PING / TIME_BETWEEN_SAMPLES // the amount of samples between satellite pings
#define SAMPLES_PER_PACKET TIME_BETWEEN_PING_SEQUENCE / TIME_BETWEEN_SAMPLES // the number of samples in a ping sequence
#define SAMPLE_PACKET_SIZE (SAMPLE_PER_PACKET * 2 + 1) // all samples takes 2 bytes and we use one byte for packetType

// the size of the largest possible packet in Protocol Layer
#define MAX_PACKET_SIZE sizeof(struct SamplePacketVerified)