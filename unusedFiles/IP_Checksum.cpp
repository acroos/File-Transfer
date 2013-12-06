#include <iostream>
#include <fstream>
using namespace std;

typedef unsigned short int u_short;
typedef unsigned long int u_long;

u_short count;

typedef struct Packet {

} pktype;



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

int main() 
{
	ifstream inFile;
	u_long inFileSize;

	inFile.open("filesIn/sample", ios::in | ios::binary);
	if (inFile.fail()) {
		cerr << "Could not open file." << endl;
		exit(1);
	}

	inFile.seekg(0, ios::end);
	inFileSize = inFile.tellg();
	inFile.seekg(0, ios::beg);

	unsigned char* dataToSend;
	dataToSend = new unsigned char[inFileSize];
	inFile.read((char *)dataToSend, inFileSize);
	inFile.close();

	printf("File size: %ld\n", inFileSize);

	//16 bit internet checksum
	u_long numShortsToSum = inFileSize / 2;
	u_short *bufferToChecksum = new u_short[numShortsToSum];


	int dataPosition = 0;
	for (u_long i = 0; i < numShortsToSum; i++) {
		bufferToChecksum[i] = (dataToSend[dataPosition] << 8) | dataToSend[dataPosition+1];
		dataPosition += 2;
	}

	for (u_long i = 0; i < inFileSize; i++) {
		printf("Data[%ld] : %x\n", i, dataToSend[i]);
	}
	
	for (u_long i = 0; i < numShortsToSum; i++) {
		printf("Data[%ld]: %x\n", i, bufferToChecksum[i]);
	}

	//checksum the sequence
	u_short checksum = Checksum(bufferToChecksum, numShortsToSum); 
	printf("Checksum: %x\n", checksum);

	ofstream outFile;
	outFile.open("filesOut/sample", ios::out | ios::binary);

	if (outFile.is_open()) {
		outFile.write((char *)dataToSend, inFileSize);
	} else {
		fprintf(stderr, "Could not write to file\n");
	}


}