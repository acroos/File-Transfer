#include <stdio.h>
#include <errno.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>`
#include <unistd.h>

#define SERVER_PORT 7654
#define PATH_MAX 256

typedef unsigned short int u_short;
typedef unsigned int u_int ;
typedef unsigned long int u_long;
typedef unsigned char byte; 

// struct to hold the byte array (packet) as two separate pieces (payload and checksum)
typedef struct {
	byte * payload ;
	byte * checksum ;
} Packet ;

// splits received packet into payload and checksum
Packet processPacket(byte * packetContents, u_int length) ;

// converts an array of byts into an array of shorts
u_short * bytes2ushorts(byte * byteArray, u_int length) ;

// calculates the checksum from an array of shorts
u_short Checksum(u_short *buf, int count) ;

// removes new line from char array
void removeNewline(char * s) ;

int main(int argc, char **argv)
{

    int socket_desc , client_sock ;
    unsigned c ;
    struct sockaddr_in server ;
    struct sockaddr_storage their_addr;
    char file_to_transfer[PATH_MAX] ;
    byte *buffer, *file ;
    u_int receiveSize = 20, payloadSize ; // Receive size initially 20 for "handshake" packet
    u_long numberOfPackets = 0, packetNum = 0, fileSize = 0, fileOffset = 0 ;
    FILE *outFile ;

    fprintf(stderr, "Please enter a filename for file to be copied to\n") ;
    if (fgets(file_to_transfer, PATH_MAX, stdin) == NULL) {
        perror("Failed to get file name");
        exit(1);
    }

    removeNewline(file_to_transfer) ;

	// build address data structure
	bzero((char *)&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(SERVER_PORT);

    //Create socket
    socket_desc = socket(PF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        perror("Could not create socket");
    }

    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("bind failed. Error");
        return 1;
    }

    //Listen
    if (listen(socket_desc , 3) == -1)
    {
        perror("listen") ;
    }

    c = sizeof(struct sockaddr_in) ;
    client_sock = accept(socket_desc, (struct sockaddr *)&their_addr, &c) ;
	if(client_sock < 0) {
		perror("Accept failed.") ;
	}

    //Accept and incoming connection
    packetNum = 0 ;
    while( 1 )
    {
    	// Re-initialize buffer to be empty ;
        buffer = (byte *)calloc (receiveSize, sizeof(byte)) ;
     
        // Receive the next packet from sender.
        if ( read(client_sock, buffer, receiveSize) == -1 ) {
        	perror("read failed.") ;
        	exit(1) ;
        }
		if (packetNum == 0) {

            fprintf(stderr, "Handshake Packet Received.\n") ;
			// First "handshake packet" sets the packet size and number of packets
			receiveSize = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3] ;
			payloadSize = receiveSize - 2 ;
			fprintf(stderr, " -- receive size: %d\n", receiveSize) ;

			// Number of packets to receive
			numberOfPackets = (buffer[4] << 56) | (buffer[5] << 48) | (buffer[6] << 40) | (buffer[7] << 32) | (buffer[8] << 24) | (buffer[9] << 16) | (buffer[10] << 8) | buffer[11] ;
			fprintf(stderr, " -- number of packets: %ld\n", numberOfPackets) ;

			// Find total file size from number of packets * packet size
			fileSize = (buffer[12] << 56) | (buffer[13] << 48) | (buffer[14] << 40) | (buffer[15] << 32) | (buffer[16] << 24) | (buffer[17] << 16) | (buffer[18] << 8) | buffer[19] ;
			fprintf(stderr, " -- file size: %ld\n", fileSize) ;

			// Set the size of the file array to the amount of packets to be received
			file = (byte *)calloc(fileSize, sizeof(byte)) ;
            fprintf(stderr, "\n") ;

			// Increment packet number to begin processing first file packet
			packetNum++ ;

		} else if(packetNum > numberOfPackets) {
			// If the last packet has been received, exit while loop
			break ;

		} else {
			// Receive and process each packet.
    		fprintf(stderr, "Packet [%ld] received\n", packetNum) ;

    		// Split up the payload and checksum of incoming packet
    		Packet incomingPacket = processPacket(buffer, receiveSize) ;

    		u_short incomingCheckSum = 0;
            u_short calculatedCheckSum = 0;

    		// Convert the incoming checksum (last two bytes of packet) from bytes to shorts
    		incomingCheckSum = (incomingPacket.checksum[0] << 8) | incomingPacket.checksum[1] ;

    		// Convert the incoming payload (all but last two bytes of packet) from bytes to shorts
    		u_short *payloadInShorts = bytes2ushorts(incomingPacket.payload, payloadSize) ;

    		// Find the number of shorts that will be converted from bytes (handle for odd numbers)
    		u_short numShorts = 0 ;
    		if (payloadSize % 2)  {
    			numShorts = ( payloadSize / 2 ) + 1 ;
    		} else {
    			numShorts = payloadSize / 2 ;
    		}

    		// Calculate the checksum from the received payload
    		calculatedCheckSum = Checksum(payloadInShorts, numShorts)  ;

    		// fprintf(stderr, "Incoming checksum: %x\n", incomingCheckSum) ;
    		// fprintf(stderr, "Calculated checksum: %x\n", calculatedCheckSum) ;

    		// If the checksums match, copy payload to file buffer and send an ACK
    		if( incomingCheckSum == calculatedCheckSum ) {

                fprintf(stderr, "Checksum matched.\n") ;

                // Calculate a byte offset for putting the payload into the buffer to be written to a file
                fileOffset = (packetNum - 1) * payloadSize ;

                // Copy the payload to the correct offset of the buffer 
                memcpy(file + fileOffset, incomingPacket.payload, payloadSize) ;

                // Send ACK
    			char good = 'A' ;
    			if ( write( client_sock, &good, 1) == -1 ) {
    				perror("ACK not sent") ;
    			}
    			fprintf(stderr, "ACK [%ld] sent.\n", packetNum) ;
                packetNum++ ;
    		} else {
    			fprintf(stderr, "Checksum did not match, ACK[%ld] not sent.\n", packetNum) ;
    		}

    		free(payloadInShorts) ;

		}

		// Free pointers to deallocate memory
		free(buffer) ;
    }

    close(client_sock) ;

    // open file for writing binary bits to 
    outFile = fopen(file_to_transfer, "wb") ;

    // if file did not open, print error and exit
    if (outFile == NULL) {
		perror("Opening file for writing");
		exit(1);
    }

    // write file buffer to outFile
    fwrite(file, sizeof(byte), fileSize, outFile) ;

    fclose(outFile) ;

    // free(outFile) ;
    free(buffer) ;
    free(file) ;

    fprintf(stderr, "Done. Received %ld bytes. \n\n\n", fileSize) ;
	return 0;
}




Packet processPacket(byte * packetContents, u_int length) {
	Packet recv ;
	byte * message = (byte *)calloc((length-2), sizeof(byte)) ;
	byte * chksum  = (byte *)calloc(2, sizeof(byte)) ;
	u_int j = 0 ;
    u_int i = 0 ;

	// store all but last two bytes as the message (payload)
	j=0 ;
	for(i=0; i<(length - 2); i++) {
		message[i] = packetContents[i] ;
	}

	// store the last two bytes as the checksum
	j=0 ;
	for(i=(length-2); i<length; i++) {
		chksum[j] = packetContents[i] ;
		// fprintf(stderr, "%02x ", packetContents[i]) ;
		j++ ;
	}
	// fprintf(stderr, "\n\n") ;

	// build struct 
	recv.payload    = message ;
	recv.checksum   = chksum ;

    free(message) ;
    free(chksum) ;

	return recv ;
}

u_short * bytes2ushorts(byte * byteArray, u_int length) {
	u_short * shortArray = (u_short *)calloc(length, sizeof(u_short)) ;
    u_long i = 0 ;

	int dataPosition = 0;
	for (i = 0; i < length; i++) {
		shortArray[i] = (byteArray[dataPosition] << 8) | byteArray[dataPosition+1];
		dataPosition += 2;
	}

	return shortArray ;
}

u_short Checksum(u_short *buf, int count)
{
	register u_long sum = 0;
	while (count--)
	{
		sum += *buf++;
		if (sum & 0xFFFF0000)
		{
			//Carry occured, wrap it around
			sum &= 0xFFFF;
			sum++;
		}
	}

	return ~(sum & 0xFFFF);
}

void removeNewline(char *s)
{
    int len = 0;
    len = strlen(s);

    if (len > 0 && s[len-1] == '\n') 
        s[len-1] = '\0';
}