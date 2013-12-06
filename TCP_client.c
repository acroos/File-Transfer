#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVER_PORT 7654

typedef unsigned char byte;
typedef unsigned short int u_short;
typedef unsigned int u_int;
typedef unsigned long int u_long;

u_long file_to_transfer_size = 0;
u_long number_of_packets = 0;
u_long packet_num = 0;

//Strips trailing white space from char array
char *strstrip(char *s);
//Removes 
void removeNewline(char *s);
u_short Checksum(u_short* buf, int count);
u_short* ShortArrayFromByteArray(byte* bytes, u_long num_bytes);
byte* BytesFromShort(u_short short_value);
byte* BytesFromInt(u_int int_value);
byte* BytesFromLong(u_long long_value);
byte* readFileIntoMemory(char *file_path);
byte* createPacket(byte* file_contents, u_int packet_size, u_long packet_num_in);
void sendHandshake(u_int packet_size, u_long num_packets, int socket);
void printPacket(byte* pkt, u_int pkt_size);


int main(int argc, char* argv[])
{

	char *host;
	char file_to_transfer[256];
	char input_buffer[10];
	char ip_buffer[16];
	u_int user_packet_size = 0;
	u_int user_timeout_option = 0;
	u_int user_timeout_value = 0;
	struct hostent *hp;
	struct sockaddr_in sin;
	int sock = 0;
	int sock_wait_fd = 0;
	fd_set set;

	//Ack Timeout
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	if (argc != 1) {
		fprintf(stderr, "Usage: client\n"); 
		exit(1);
	}

	fprintf(stderr, ">>> Enter a file name to transfer (relative to your directory): ");
	if (fgets(file_to_transfer, PATH_MAX, stdin) == NULL) {
		perror("Failed to get file name");
		exit(1);
	}

	removeNewline(file_to_transfer);

	fprintf(stderr, ">>> Enter a packet size: ");
	fgets(input_buffer, 10, stdin);
	user_packet_size = strtoul(input_buffer, NULL, 0);
	memset(input_buffer, 0, 10);

	fprintf(stderr, ">>> Where to? (IP address): ");
	fgets(ip_buffer, 16, stdin);

	fprintf(stderr, ">>> Enter timeout option, 1 (user specified) or 2 (ping calculated): ");
	fgets(input_buffer, 10, stdin);
	user_timeout_option = atoi(input_buffer);
	memset(input_buffer, 0, 10);

	if (user_timeout_option == 1) {

		fprintf(stderr, ">>> Please enter a timeout in seconds: ");
		fgets(input_buffer, 10, stdin);
		user_timeout_value = atoi(input_buffer);
		tv.tv_sec = user_timeout_value;

	} else if (user_timeout_option == 2) {
		fprintf(stderr, "\n-- The packet timeout will be ping calculated\n");
	} else {
		fprintf(stderr, "\n-- Invalid option for timeout. Exiting...\n");
		exit(1);
	}

	

	byte* file_bytes = readFileIntoMemory(file_to_transfer);

	u_int payload_size = user_packet_size - 2;
	if (file_to_transfer_size % payload_size) {
		number_of_packets = (file_to_transfer_size / payload_size) + 1;
	} else {
		number_of_packets = file_to_transfer_size / payload_size;
	}

	removeNewline(ip_buffer);

	fprintf(stderr, "\n  TRANSFERING ... \n"); 
	fprintf(stderr, "  -----------------------------------------------------------------\n"); 
	fprintf(stderr, "    TO:                [ %s ]\n", ip_buffer); 
	fprintf(stderr, "    FILE:              [ %s ]\n", file_to_transfer); 
	fprintf(stderr, "    FILE SIZE:         [ %ld ]\n", file_to_transfer_size); 
	fprintf(stderr, "    PACKET SIZE:       [ %d ]\n", user_packet_size);
	fprintf(stderr, "    NUMBER OF PACKETS: [ %ld ]\n", number_of_packets);
	fprintf(stderr, "    TIMEOUT:           [ %d ]\n", user_timeout_value);
	fprintf(stderr, "  -----------------------------------------------------------------\n\n"); 

	host = ip_buffer;
	if ((hp = gethostbyname(host)) == NULL) {
		herror("Invalid host");
		exit(1);
	}

	// build address data structure
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = htons(SERVER_PORT);

	//Create and connect to socket
	fprintf(stderr, "  Creating socket ...\n");
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("  Could not create socket");
		exit(1);
	}

	FD_ZERO(&set);
	FD_SET(sock, &set);

	fprintf(stderr, "  Connecting to socket ...\n\n");
	if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("  Could not connect to socket");
		close(sock);
		exit(1);
	}

	//Send handshake to server
	sendHandshake(user_packet_size, number_of_packets, sock);

	//Loop through, create packets and send
	int count = 0;
	while (packet_num < number_of_packets) {
		char* ack;

		byte* packet = createPacket(file_bytes, user_packet_size, packet_num);
		printPacket(packet, user_packet_size);

		/*if (count == 0) {
			packet[3] = 'a';
			count++;
		}*/

		ssize_t bytes_sent = write(sock, packet, user_packet_size);
		if (bytes_sent == -1) {
			perror(" Recieve Error");
			exit(1);
		}

		fprintf(stderr, "# Bytes sent: %11d\n", (int)bytes_sent);
		free(packet);

		sock_wait_fd = select(sock + 1, &set, NULL, NULL, &tv);
		if(sock_wait_fd == -1) {
			perror("Select error");
		} else if (sock_wait_fd == 0) {
			fprintf(stderr, "\n\n# *** TIMEOUT: ACK NOT RECIEVED ***\n\n");
			fprintf(stderr, " Resending Packet %ld\n\n", packet_num + 1);
			FD_ZERO(&set);
			FD_SET(sock, &set);
		} else {
			ssize_t ackTest = read(sock, &ack, 1);
			if (ackTest == -1) {
				perror(" Recieve Error");
				exit(1);
			}
			packet_num++;
			printf("# Recieved ACK\n\n");
			FD_ZERO(&set);
			FD_SET(sock, &set);
		}

	}

	fprintf(stderr, "Number of packets sent: %ld\n\n", number_of_packets);


	free(file_bytes);
	close(sock);
	fflush(stdout);
	exit(0);
}

u_short Checksum(u_short *buf, int count)
{
	register u_long sum = 0;
	while (count--)
	{
		sum += *buf;
		buf++;
		if (sum & 0xFFFF0000)
		{
			//Carry occured, wrap it around
			sum &= 0xFFFF;
			sum++;
		}
	}
	return ~(sum & 0xFFFF);
}

u_short* ShortArrayFromByteArray(byte* bytes, u_long num_bytes) 
{
	u_long num_shorts_to_sum = num_bytes / 2;
	u_short *buffer_to_checksum = (u_short *)calloc(num_shorts_to_sum, sizeof(u_short));


	int dataPosition = 0;
	u_long i = 0;
	for (i = 0; i < num_shorts_to_sum; i++) {
		buffer_to_checksum[i] = (bytes[dataPosition] << 8) | bytes[dataPosition+1];
		dataPosition += 2;
	}

	return buffer_to_checksum;
}

byte* BytesFromShort(u_short short_value)
{
	byte* bytes = (byte *)calloc(2, sizeof(byte));
	bytes[1] = short_value & 0xFF;
	bytes[0] = (short_value >> 8) & 0xFF;
	return bytes; 
}

byte* BytesFromInt(u_int int_value)
{
	byte* bytes = (byte *)calloc(4, sizeof(byte));
	bytes[3] = int_value & 0xFF;
	bytes[2] = (int_value >> 8) & 0xFF;
	bytes[1] = (int_value >> 16) & 0xFF;
	bytes[0] = (int_value >> 24) & 0xFF;
	return bytes; 
}

byte* BytesFromLong(u_long long_value)
{
	byte* bytes = (byte *)calloc(8, sizeof(byte));
	bytes[7] = long_value & 0xFF;
	bytes[6] = (long_value >> 8) & 0xFF;
	bytes[5] = (long_value >> 16) & 0xFF;
	bytes[4] = (long_value >> 24) & 0xFF;
	bytes[3] = (long_value >> 32) & 0xFF;
	bytes[2] = (long_value >> 40) & 0xFF;
	bytes[1] = (long_value >> 48) & 0xFF;
	bytes[0] = (long_value >> 56) & 0xFF;
	return bytes; 
}

byte* readFileIntoMemory(char *file_path) {

	FILE *fp;

	fp = fopen(file_path, "rb");
	//fp = fopen("filesIn/cksum", "rb");
	if (fp == NULL) {
		fprintf(stderr, "Cannot open file. Perhaps it doesn't exist?\n");
		exit(1);
	}

	if (fseek(fp, 0, SEEK_END) == -1) {
		perror("Failed to seek");
	}
	if ((file_to_transfer_size = ftell(fp)) == -1) {
		perror("Failed to get files size");
	}
	rewind(fp);

	byte* data_to_send;
	data_to_send = (byte *)calloc(file_to_transfer_size, sizeof(byte));
	if (data_to_send == NULL) {
		perror("Failed to allocate data [ data_to_send ]");
		exit(1);
	}

	size_t result;
	result = fread(data_to_send, 1, file_to_transfer_size, fp);
	if (result != file_to_transfer_size) {
		fprintf(stderr, "Error reading file\n");
		exit(1);
	}

	fclose(fp);
	return data_to_send;

}

byte* createPacket(byte* file_contents, u_int packet_size, u_long packet_num_in) {

	u_int payload_size = packet_size - 2;

	//Allocate packet memory
	byte* packet = (byte *)calloc(packet_size, sizeof(byte));
	memset(packet, 0, packet_size);

	//Allocate payload memory
	byte* payload_bytes = (byte *)calloc(payload_size, sizeof(byte));
	memcpy(payload_bytes, file_contents + (packet_num * payload_size), payload_size);

	//Make short array to checksum
	u_short* shorts_to_checksum = ShortArrayFromByteArray(payload_bytes, payload_size);
	
	//Print checksum shorts
	u_int num_shorts_to_sum = payload_size / 2;

	//Calculate checksum
	u_short checksum = Checksum(shorts_to_checksum, num_shorts_to_sum);
	byte* checksum_bytes = BytesFromShort(checksum);

	//Populate packet
	memcpy(packet, payload_bytes, payload_size);
	memcpy(packet + payload_size, checksum_bytes, 2);

	free(payload_bytes);
	free(shorts_to_checksum);
	free(checksum_bytes);

	return packet;
}

void sendHandshake(u_int packet_size, u_long num_packets, int socket) {

	byte* to_server_packet_size = (byte *)calloc(4, sizeof(byte));
	byte* to_server_num_packets = (byte *)calloc(8, sizeof(byte));
	byte* to_server_file_size = (byte *)calloc(8, sizeof(byte));

	to_server_packet_size = BytesFromInt(packet_size);
	to_server_num_packets = BytesFromLong(num_packets);
	to_server_file_size = BytesFromLong(file_to_transfer_size);

	byte* handshake = (byte *)calloc(20, sizeof(byte));
	memcpy(handshake, to_server_packet_size, 4);
	memcpy(handshake + 4, to_server_num_packets, 8);
	memcpy(handshake + 12, to_server_file_size, 8);

	fprintf(stderr, "# Handshake\n");
	int i = 0;
	fprintf(stderr, "# [ ");
	for(i = 0; i < 20; i++) {
		fprintf(stderr, "%02x", handshake[i]);
	}
	fprintf(stderr, " ]\n\n");


	ssize_t bytesSent = write(socket, handshake, 20);
	if (bytesSent == -1) {
		perror("Failed to send packet size");
		exit(1);
	}

	free(to_server_packet_size);
	free(to_server_num_packets);
	free(to_server_file_size);
	free(handshake);

}

char* strstrip(char *s) {
	size_t size;
	char *end;

	size = strlen(s);

	if (!size)
	     return s;

	end = s + size - 1;
	while (end >= s && isspace(*end))
	     end--;
	*(end + 1) = '\0';

	while (*s && isspace(*s))
	     s++;

	return s;
}

void removeNewline(char *s)
{
	int len = 0;
    len = strlen(s);

    if (len > 0 && s[len-1] == '\n') 
        s[len-1] = '\0';

}

void printPacket(byte* pkt, u_int pkt_size) {

	fprintf(stderr, "# PACKET\n");
	fprintf(stderr, "# Number sent: %10ld\n", packet_num + 1);
	/*fprintf(stderr, "# Payload: ");
	u_int i = 0;
		for (i = 0; i < pkt_size; i++) {
			fprintf(stderr, "%02x", pkt[i]);
			if ((i % 30 == 0)  && (i != 0)) {
				fprintf(stderr, "\n# ");
			}
		}
		fprintf(stderr, "\n");*/
		
}
