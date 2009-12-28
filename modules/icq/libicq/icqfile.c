/* ICQ File transfer PRE-RELEASE BETA v.0001
 * by Eric Hanson (hanser@wwc.edu), Sam Fortiner (fortsa@cs.wwc.edu),
 * Hans Buchheim (buchha@cs.wwc.edu), and Rick Patchett (patcri@cs.wwc.edu).
 * 
 * We're working on a back end to an ICQ client that others can build
 * front-ends for.  Here's just a tiny bit of what we're working towards,
 * just in case you might find it valuable.
 * 
 * This is our first attempt at anything icq related.  It's very messy,
 * and we plan to clean it up quite a bit before we release it, so if you
 * come across this file, check http://moonbase.wwc.edu:8004/projects/icq/
 * for a newer version, cause this is just a pre-release for ICQ
 * developers to make this->comments about.  Don't pass this on.  Thanks.
 *
 * Known bugs:
 *   - Doesn't support speed changes.
 *   - Doesn't work quite right with the Java client
 *
 * Planned Enhancements:
 *   - Remote Filename
 *   - Message sending, URL sending, Chatting (?)
 *   - Stability
 *   - Clean Code
 * 
 * Tested with:
 *   - from Slackware Linux 2.0.30 to ICQ Version 98a Beat/Win95
 * 
 * Compile Options:
 *   None.  Just type "g++ icqfile.cpp -o icqfile"
 *
 * Thanks to:
 *    Magnus Ihse, CMB, Seth McGann.
 * 
 * Copyright (c) 1998
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

#define ICQ_FILE_VERSION htons(0x0300)
#define COMMAND_SENDFILE htons(0xee07)

typedef struct _ICQPacket {
	/* Packet 2 Header */
	unsigned long SourceUIN;
	unsigned short ICQVersion;
	unsigned long Command;
	unsigned short CommentLength;
	unsigned long SenderIP;
	unsigned short SenderPort;
	unsigned long Unknown1b;
	unsigned long Unknown2b;
	unsigned long FileNameLength;
	unsigned char Unknown4b;
	unsigned long FileSize;
	unsigned long Unknown1c;
	unsigned long Unknown2c;	/* Sender status ? */
	short FirstHeaderSize;
	unsigned short SecondHeaderSize;

	unsigned short DestinationPort;

	char header[8];
	char header2[3];
	char intPad[4];
	char shortPad[2];
	char charPad[1];

	char fileName[1024];
	char comment[1024];
} ICQPacket;

void exchangeName(ICQPacket *this, int sock);
void writePacket(ICQPacket *this, int sock);
void readResponse(ICQPacket *this, int sock, ICQPacket *pPkt);
void readNameExchange(ICQPacket *this, int sock, ICQPacket *pPkt);
void sendFilePreamble(ICQPacket *this, int sock);
void readFilePreamble(ICQPacket *this, int sock);
void sendFile(ICQPacket *this, int sock);

void initilizePKT(ICQPacket *pkt)
{
	pkt->FirstHeaderSize = 0x1a;
	pkt->header[0] = 0xff;
	pkt->header[1] = 0x03;
	pkt->header[2] = 0x00;
	pkt->header[3] = pkt->header[4] = pkt->header[5]
		= pkt->header[6] = pkt->header[7]
		= pkt->header[8] = 0x00;

	pkt->header2[0] = 0x04;
	pkt->header2[1] = pkt->header2[2] = 0x00;
	pkt->intPad[0] = pkt->intPad[1] = pkt->intPad[2] = pkt->intPad[3] =
		0x00;
	pkt->shortPad[0] = pkt->shortPad[1] = 0x00;
	pkt->charPad[0] = 0x00;
}

void writePacket(ICQPacket *this, int sock)
{
	char buffer[65537];
	int offset = 0;

	memcpy(buffer, this->header, 9);
	offset += 9;
	memcpy(buffer + offset, &(this->SourceUIN), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SenderIP), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SenderIP), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SenderPort), sizeof(unsigned short));
	offset += sizeof(unsigned short);
	memcpy(buffer + offset, &(this->header2), 3);
	offset += 3;

	offset += sizeof(unsigned short);

	memcpy(buffer + offset, &(this->SourceUIN), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->ICQVersion), sizeof(unsigned short));
	offset += sizeof(unsigned short);
	memcpy(buffer + offset, &(this->Command), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SourceUIN), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->ICQVersion), sizeof(unsigned short));
	offset += sizeof(unsigned short);
	memcpy(buffer + offset, &(this->CommentLength), sizeof(unsigned short));
	offset += sizeof(unsigned short);

	memcpy(buffer + offset, this->comment, strlen(this->comment));
	offset += strlen(this->comment);
	buffer[offset++] = 0x0;

	memcpy(buffer + offset, &(this->SenderIP), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SenderIP), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SenderPort), sizeof(unsigned short));
	offset += sizeof(unsigned short);
	memcpy(buffer + offset, &(this->Unknown1b), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->Unknown2b), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->FileNameLength), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->Unknown4b), sizeof(unsigned char));
	offset += sizeof(unsigned char);

	memcpy(buffer + offset, this->fileName, strlen(this->fileName));
	offset += strlen(this->fileName);
	buffer[offset++] = 0x0;

	memcpy(buffer + offset, &(this->FileSize), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->Unknown1c), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->Unknown2c), sizeof(unsigned long));
	offset += sizeof(unsigned long);

	this->SecondHeaderSize = offset - this->FirstHeaderSize - 2;
	memcpy(buffer + 0x1a, &(this->SecondHeaderSize),
		sizeof(unsigned short));

	write(sock, (void const *)&(this->FirstHeaderSize),
		sizeof(unsigned short));

	write(sock, (void const *)buffer, offset);

}

void exchangeName(ICQPacket *this, int sock)
{
	char local_header[] = { 0xff, 0x03, 0x00, 0x00, 0x00 };
	char local_header2[] =
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
	char local_header3[] = { 0x64, 0x00, 0x00, 0x00 };

	// local_this->header3 is the speed in percent (0 - 100)
	// bytes 6 thru 9 of local_this->header2 is the number of files to send.

	char buffer[65537];
	int offset = 0;

	// make username equal to UIN
	char userName[1024];
	unsigned short userNameLength;

	this->FirstHeaderSize = 0x1a;

	memcpy(buffer, &local_header, 5);
	offset += 5;
	this->SenderPort--;
	memcpy(buffer + offset, &(this->SenderPort), sizeof(unsigned short));

	offset += sizeof(unsigned short);
	offset += 2;		// skip over 0x00, 0x00

	memcpy(buffer + offset, &(this->SourceUIN), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SenderIP), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SenderIP), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &(this->SenderPort), sizeof(unsigned short));
	offset += sizeof(unsigned short);
	memcpy(buffer + offset, &(this->header2), 3);
	offset += 3;

	this->SenderPort++;	// set senderport back to actual port

	offset += 2;		// skip over rest of file length.

	memcpy(buffer + offset, &local_header2, 9);
	offset += 9;
	memcpy(buffer + offset, &(this->FileSize), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &local_header3, 4);
	offset += 4;

	sprintf(userName, "%ld", this->SourceUIN);
	userNameLength = strlen(userName);

	memcpy(buffer + offset, &userNameLength, sizeof(unsigned short));
	offset += sizeof(unsigned short);
	memcpy(buffer + offset, &userName, userNameLength);
	offset += userNameLength;
	buffer[offset++] = 0x0;

	this->SecondHeaderSize = offset - this->FirstHeaderSize - 2;
	memcpy(buffer + 0x1a, &(this->SecondHeaderSize),
		sizeof(unsigned short));

	write(sock, (void const *)&(this->FirstHeaderSize),
		sizeof(unsigned short));

	write(sock, (void const *)buffer, offset);

	return;
}

/* Set up this->fileName, fileSize, 
 * */
void sendFilePreamble(ICQPacket *this, int sock)
{
	char local_header[] = { 0x02, 0x00 };
	char local_header2[] = { 0x01, 0x00, 0x00 };	// file number?
	char buffer[65537];
	unsigned long offset = 0, speed = 100, fileNameLength =
		strlen(this->fileName) + 1;
	unsigned long Unknown1 = 0x0;
	unsigned short nextPacketSize;

	memcpy(buffer + offset, &local_header, 2);
	offset += 2;
	memcpy(buffer + offset, &(fileNameLength), sizeof(unsigned short));
	offset += sizeof(unsigned short);
	memcpy(buffer + offset, &(this->fileName), fileNameLength - 1);
	offset += fileNameLength - 1;
	buffer[offset++] = 0x0;
	memcpy(buffer + offset, local_header2, 3);
	offset += 3;
	memcpy(buffer + offset, &(this->FileSize), sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &Unknown1, sizeof(unsigned long));
	offset += sizeof(unsigned long);
	memcpy(buffer + offset, &speed, sizeof(unsigned long));
	offset += sizeof(unsigned long);

	nextPacketSize = (unsigned short)offset;
	write(sock, (void *)&nextPacketSize, sizeof(unsigned short));
	write(sock, (void *)buffer, nextPacketSize);

}

void readNameExchange(ICQPacket *this, int sock, ICQPacket *pPkt)
{
	unsigned short nextPacketSize;
	int offset = 0;
	int connectionSpeed = 0;
	char constant;
	char *buffer;
	read(sock, (void *)&nextPacketSize, sizeof(unsigned short));

	buffer = malloc(nextPacketSize);
	if (buffer) {
		read(sock, (void *)buffer, nextPacketSize);

		memcpy(&constant, buffer + offset, 1);
		offset += 1;

		memcpy(&connectionSpeed, buffer + offset,
			sizeof(unsigned long));
		offset += sizeof(unsigned long);
		memcpy(&pPkt->CommentLength, buffer + offset,
			sizeof(unsigned short));
		offset += sizeof(unsigned short);
		memcpy(&pPkt->comment, buffer + offset, pPkt->CommentLength);
		offset += pPkt->CommentLength;
		pPkt->comment[pPkt->CommentLength] = 0x0;

		printf("Sending file to %s:", pPkt->comment);
		fflush(stdout);

		free(buffer);
	} else {
		printf("Error allocating buffer in readNameExchange.\n");
	}
}

void readFilePreamble(ICQPacket *this, int sock)
{
	unsigned short nextPacketSize;
	unsigned long unknown1, unknown2;
	int offset = 0;
	int connectionSpeed = 0;
	char constant;
	char *buffer;
	read(sock, (void *)&nextPacketSize, sizeof(unsigned short));

	buffer = malloc(nextPacketSize);
	if (buffer) {
		read(sock, (void *)buffer, nextPacketSize);

		memcpy(&constant, buffer + offset, 1);
		offset += 1;

		memcpy(&unknown1, buffer + offset, sizeof(unsigned long));
		offset += sizeof(unsigned long);
		memcpy(&unknown2, buffer + offset, sizeof(unsigned long));
		offset += sizeof(unsigned short);
		memcpy(&connectionSpeed, buffer + offset,
			sizeof(unsigned long));
		offset += sizeof(unsigned long);

		free(buffer);
	} else {
		printf("Error allocating buffer in readNameExchange.\n");
	}

}

void sendFile(ICQPacket *this, int sock)
{
	char *fileBuffer = malloc(this->FileSize);
	unsigned char constant = 0x06;
	int remainingBytes = this->FileSize, offset = 0;
	unsigned short nextPacketSize;

	if (fileBuffer) {
		int fd = open(this->fileName, O_RDONLY);
		if (fd == -1) {
			perror("open in sendFile");
		} else {
			int status = read(fd, fileBuffer, this->FileSize);
			if (status == -1) {
				perror("read in sendfile");
			} else {
				int firstPacket = 1;
				fflush(stdout);
				do {
					char b[2051];
					unsigned short temp;

					if (remainingBytes < 2048)
						nextPacketSize = remainingBytes;
					else
						nextPacketSize = 2048;

					if (firstPacket) {
						firstPacket = 0;
						temp = nextPacketSize + 1;
						write(sock, &temp,
							sizeof(unsigned short));
						b[0] = constant;
						memcpy(&b[1],
							fileBuffer + offset,
							nextPacketSize);
						offset += nextPacketSize;
						remainingBytes -=
							nextPacketSize;
						write(sock, b,
							nextPacketSize + 1);
					} else {
						temp = nextPacketSize + 1;
						memcpy(b, &temp,
							sizeof(unsigned short));
						b[2] = constant;
						memcpy(&b[3],
							fileBuffer + offset,
							nextPacketSize);
						offset += nextPacketSize;
						remainingBytes -=
							nextPacketSize;
						write(sock, b,
							nextPacketSize + 3);
					}
					printf(".");
					fflush(stdout);
				} while (remainingBytes);

				printf("\nFile sent.\n");

			}
			free(fileBuffer);
		}
	} else {
		printf("Error allocating memory for fileBuffer in sendFile.\n");
	}
}

void readResponse(ICQPacket *this, int sock, ICQPacket *pPkt)
{
	unsigned short nextPacketSize;
	int offset = 0;
	char *buf;
	unsigned short FileLength;
	read(sock, (void *)&nextPacketSize, sizeof(unsigned short));

	buf = malloc(nextPacketSize);
	if (buf) {
		unsigned long reject;

		read(sock, (void *)buf, nextPacketSize);

		memcpy(&pPkt->SourceUIN, &buf[offset], sizeof(unsigned long));
		offset += sizeof(unsigned long);
		memcpy(&pPkt->ICQVersion, &buf[offset], sizeof(unsigned short));
		offset += sizeof(unsigned short);
		if (pPkt->ICQVersion != ICQ_FILE_VERSION)
			printf("Version differences:  target 0x%x, actual 0x%x.\n", ICQ_FILE_VERSION, pPkt->ICQVersion);
		memcpy(&pPkt->Command, &buf[offset], sizeof(unsigned long));
		offset += sizeof(unsigned long);
		offset += sizeof(unsigned long);	// skip over the 2nd UIN
		offset += sizeof(unsigned short);	// skip over the 2nd version
		memcpy(&pPkt->CommentLength, &buf[offset],
			sizeof(unsigned short));
		offset += sizeof(unsigned short);
		memcpy(&pPkt->comment, &buf[offset], pPkt->CommentLength);
		offset += pPkt->CommentLength;

		memcpy(&pPkt->SenderIP, &buf[offset], sizeof(unsigned long));
		offset += sizeof(unsigned long);
		offset += sizeof(unsigned long);	// skip over 2nd IP
		memcpy(&pPkt->SenderPort, &buf[offset], sizeof(unsigned short));
		offset += sizeof(unsigned short);
		offset += 3;	// skip junk 0x00, 0x00, 0x04
		memcpy(&reject, &buf[offset], sizeof(unsigned long));
		offset += sizeof(unsigned long);

		if (!reject)
			printf("Connection accepted.\n");
		else {
			printf("Connection REJECTED.\n");
			printf("Comment: %s\n", pPkt->comment);
		}

		offset += sizeof(unsigned short);	// skip over dest port

		offset += sizeof(unsigned short);	// skip 2 bytes 0x00, 0x00

		//memcpy(&pPkt->FileNameLength, &buf[offset], sizeof(unsigned short));
		memcpy(&FileLength, &buf[offset], sizeof(unsigned short));
		pPkt->FileNameLength = FileLength;
		offset += sizeof(unsigned short);

		fprintf(stderr, "Copying %ld bytes to copy %s\n",
			pPkt->FileNameLength, &buf[offset]);

		memcpy(&pPkt->fileName, &buf[offset], pPkt->FileNameLength);
		offset += pPkt->FileNameLength;

		offset += sizeof(unsigned long);	// skip 4 bytes 0x00, 0x00, 0x00, 0x00

		memcpy(&pPkt->DestinationPort, &buf[offset],
			sizeof(unsigned short));
		offset += sizeof(unsigned short);

		free(buf);

	} else {
		printf("Error allocating memory.\n");
	}
}

int ICQSendFile(const char *ip, const char *port, const char *uin,
	const char *fileName, const char *comment)
{
	struct sockaddr_in sin, sout, sin2, sout2;
	struct stat st;

	int sock, x;
	int nameSize;
	int fileSize = 0;
	int sock2;
	ICQPacket pkt, pkt2, pkt3;
	initilizePKT(&pkt);
	initilizePKT(&pkt2);
	initilizePKT(&pkt3);

	fprintf(stderr, "ip = %s port = %s\n", ip, port);

	//comment[0] = 0;

//      {
//              printf(" ICQ File Spoofer\n");
//              printf("usage: %s ip port SpoofedUIN file \"comment\"\n", argv[0]);
//  }

	if (stat(fileName, &st) != -1) {
		fileSize = st.st_size;
	} else {
		perror("stat");
		/* exit(1); */
	}

	/* make a socket */
	if (!(sock = socket(AF_INET, SOCK_STREAM, 0))) {
		perror("socket");
		return (0);
	}

	/* set some stuff up */
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(ip);
	sin.sin_port = htons(atol(port));

	/* connect to the victim */
	if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		perror("connect");
		return (0);
	}

	nameSize = sizeof(struct sockaddr);
	getsockname(sock, (struct sockaddr *)&sout, &nameSize);

	/* make our payload */
	x = -1;

	pkt.SourceUIN = atoi(uin);
	pkt.ICQVersion = ICQ_FILE_VERSION;
	pkt.Command = COMMAND_SENDFILE;
	pkt.CommentLength = strlen(comment) + 1;

	pkt.Unknown1b = 0x00040000;
	pkt.Unknown2b = 0x00001000;
	pkt.FileNameLength = htonl(strlen(fileName) + 1);

	pkt.Unknown4b = 0x00;

	pkt.FileSize = fileSize;
	pkt.Unknown1c = 0x0000;
	pkt.Unknown2c = 0xFFFFFFA0;

	strcpy(pkt.fileName, fileName);
	strcpy(pkt.comment, comment);

	writePacket(&pkt, sock);
	printf("Waiting for acceptance.\n");
	readResponse(&pkt, sock, &pkt2);

	/* make a socket */
	if (!(sock2 = socket(AF_INET, SOCK_STREAM, 0))) {
		perror("socket2");
		return (0);
	}

	/* set some stuff up */
	sin2.sin_family = AF_INET;
	sin2.sin_addr.s_addr = inet_addr(ip);
	sin2.sin_port = htons(pkt2.DestinationPort);

	/* connect to the victim */
	if (connect(sock2, (struct sockaddr *)&sin2, sizeof(sin2)) == -1) {
		perror("connect");
		return (0);
	}

	nameSize = sizeof(struct sockaddr);
	getsockname(sock2, (struct sockaddr *)&sout2, &nameSize);

	pkt3.SenderIP = sout2.sin_addr.s_addr;
	pkt3.SenderPort = ntohs(sout2.sin_port);
	pkt3.SourceUIN = atoi(uin);
	pkt3.FileSize = fileSize;

	exchangeName(&pkt3, sock2);

	readNameExchange(&pkt3, sock2, &pkt2);

	pkt3.FileSize = fileSize;
	strcpy(pkt3.fileName, fileName);
	sendFilePreamble(&pkt3, sock2);

	readFilePreamble(&pkt3, sock2);

	sendFile(&pkt3, sock2);

	close(sock2);

	close(sock);
	return (0);
}
