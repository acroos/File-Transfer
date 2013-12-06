#include <cstdlib>
#include <sstream>
#include <ctime>
#include <iostream>
#include <fstream>
#include <string>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
using namespace std;
using boost::asio::ip::tcp;

typedef unsigned char byte;
typedef unsigned short int u_short;
typedef unsigned int u_int;
typedef unsigned long int u_long;

class Packet {
	public:
		u_long packetNum ; 
		u_long packetSize ;
		byte *payload ;
		u_long checksum ;

		Packet() {
			packetNum = 1 ;
			packetSize = 1024 ;
			payload = new byte[packetSize - 8] ;
			checksum = 0x00 ;
		}

		Packet(u_int packet_number, u_short packet_size) {
			packetNum = packet_number ;
			packetSize = packet_size ;
			payload = new byte[packet_size - 8] ;
			checksum = 0x00 ;
		}

		Packet(string contents) {
			parsePacketString(contents);
		}

	private: 
		void parsePacketString(string contents) {
			int pkt_size = contents.size() ;
			int payload_size = pkt_size - 8 ;

			stringstream ss_packet_num, ss_packet_size, ss_checksum ;
			ss_packet_num.str(contents.substr(0, 4)) ;
			ss_packet_num >> hex >> packetNum ;
			fprintf(stderr, "Packet Number: %x\n", packetNum) ;

			ss_packet_size.str(contents.substr(4, 2)) ;
			ss_packet_size >> hex >> packetSize ;
			fprintf(stderr, "Packet Size: %x\n", packetSize) ;

			ss_checksum.str(contents.substr(pkt_size - 2, 2)) ;
			ss_checksum >> hex >> checksum ;
			fprintf(stderr, "Checksum: %x\n", checksum) ;

			const char * pay_load = contents.substr(6, payload_size).c_str() ;
			payload = new byte[payload_size] ;
			payload = (byte *)pay_load ;
		}

} ;

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

u_short* ShortArrayFromByteArray(byte* bytes, u_long numBytes) 
{
	u_long numShortsToSum = numBytes / 2;
	u_short *bufferToChecksum = new u_short[numShortsToSum];


	int dataPosition = 0;
	for (u_long i = 0; i < numShortsToSum; i++) {
		bufferToChecksum[i] = (bytes[dataPosition] << 8) | bytes[dataPosition+1];
		dataPosition += 2;
	}

	return bufferToChecksum;
}

Packet* createPacketsFromFile(string fileContent, int pktSize) 
{
	int fileSize = fileContent.size();
	cout << "fileContent: " << fileContent << endl;
	cout << "File size in func: " << fileSize << endl;
	int payLoadSize = pktSize - 8;
	int numPackets = fileSize / payLoadSize;
	printf("Num packets: %d\n", numPackets);

	/*u_long packetNum ; 
	u_long packetSize ;
	byte *payload ;
	u_long checksum ;*/

	Packet *packetArray = new Packet[numPackets];
	int packetIndex = 0;
	int fileIndex = 0;
	u_long packetNum = 1;

	for (int i = 0; i < numPackets; i++) {
		Packet *packet = new Packet();

		packet->packetNum = packetNum;
		packet->packetSize = pktSize;

		char *payLoad = new char[payLoadSize];
		strcpy(payLoad, fileContent.substr(fileIndex, payLoadSize).c_str());

		printf("Packey payload: %s\n", payLoad);
		packet->payload = (byte *)payLoad;

		u_short* shortsToSum = ShortArrayFromByteArray(packet->payload, payLoadSize);

		u_short checksum = Checksum(shortsToSum, payLoadSize);

		printf("Checksum %x\n", checksum);

		packetArray[i] = *packet;
		
		fileIndex += payLoadSize;
		packetIndex++;
		packetNum++;
	}

	return packetArray;

}

ifstream::pos_type size;

string readFileIntoMemory() {

	ifstream fileIn("filesIn/cksum", ios::in | ios::binary);

	char* fileInBuffer;

	if (fileIn.is_open()) {
		fileIn.seekg(0, ios::end);
		int fileInSize = fileIn.tellg();
		fileIn.seekg(0, ios::beg);

		cout << "File size: " << fileInSize << endl;
		fileInBuffer = new char [fileInSize];
		fileIn.read(fileInBuffer, fileInSize);
		fileIn.close();
	} else {
		std::cout << "Unable to open file";
	}

	printf("Reading file: %s\n", fileInBuffer);

	string messageToSend = fileInBuffer;
	cout << messageToSend << endl;

	return messageToSend;
}

int main(int argc, char* argv[])
{

	boost::system::error_code write_error;
	int userPacketSize;
	boost::array<char, 16> buffer;

	try
	{
		if (argc != 3) {
			std::cerr << "Usage: client <host> <port>" << std::endl;
			return 1;
		}

		cout << "Enter a packet size (bytes) : ";
		cin >> userPacketSize;

		string fileContents = readFileIntoMemory();
		Packet *packets = createPacketsFromFile(fileContents, 16);

		for(int i = 0; i < 1; i++) {
			printf("Packet num: %d\n", packets[i].packetNum);
			printf("Packet size: %d\n", packets[i].packetSize);
			printf("Packet payload: %s\n", packets[i].payload);
			printf("Packet checksum: %d\n", packets[i].checksum);
		}

		//boost::asio::buffer packet(packet[0], 16);

		boost::asio::io_service io_service;
		tcp::socket socket(io_service);

		/*socket.connect(
			boost::asio::ip::tcp::endpoint(
				boost::asio::ip::address::from_string(argv[1]),
				boost::lexical_cast<unsigned>(argv[2])
				)
			);

		const size_t bytesSent = boost::asio::write(socket, boost::asio::buffer(fileContents), write_error);
		cout << "Number of bytes sent: " << bytesSent << endl;*/


	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}