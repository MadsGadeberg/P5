// Global constants
#define GROUP 20
#define TIME_BETWEEN_SAMPLES 5	//
#define TIME_BETWEEN_PING 24	// Time between each satellite ping
#define TIME_BETWEEN_PING_SEQUENCE 200 // Time between each time we will ping all sattelites connected

#define SAMPLES_BETWEEN_PINGS TIME_BETWEEN_PING / TIME_BETWEEN_SAMPLES // the amount of samples between satellite pings
#define SAMPLES_PER_PACKET TIME_BETWEEN_PING_SEQUENCE / TIME_BETWEEN_SAMPLES // the number of samples in a ping sequence

// packet size without verified bit in each sample
#define SAMPLE_PACKET_SIZE (SAMPLES_PER_PACKET * 2 + 1) // all samples takes 2 bytes and we use one byte for packetType
// packet size with verified bit in each sample
#define SAMPLE_PACKET_VERIFIED_SIZE (SAMPLES_PER_PACKET * 3)