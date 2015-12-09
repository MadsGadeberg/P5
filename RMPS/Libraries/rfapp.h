// Global constants
#define GROUP 20
#define TIME_BETWEEN_SAMPLES 5	//
#define TIME_BETWEEN_PING 20	// Time between each satellite ping
#define TIME_BETWEEN_PING_SEQUENCE 200 // Time between each time we will ping all sattelites connected
#define SAMPLE_PACKET_SIZE TIME_BETWEEN_PING_SEQUENCE / TIME_BETWEEN_SAMPLES	// the number of samples in a ping sequence
#define SAMPLES_BETWEEN_PINGS TIME_BETWEEN_PING / TIME_BETWEEN_SAMPLES // the amount of samples between satellite pings

