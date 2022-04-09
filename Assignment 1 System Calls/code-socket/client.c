#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "Hello from client";
	char buffer[BUFF_SIZE] = {0};

	/* [C1]
	 * Explaint the following here.
	 * To create a socket in the specfied domain and of specified type.
	 * socket() returns a descriptor which can be used further.
	 * Address Family - AF_INET (this is IP version 4) Type - SOCK_STREAM (this means connection oriented TCP protocol) 
	 * Protocol - 0 [ or IPPROTO_IP This is IP protocol]
	 * if socket() return "-1",print error and exit.
	 * included in sys/socket.h header   
	 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	/* [C2]
	 * Explaint the following here.
	 * Set the socket information (struct sockaddr_in to 0).
	 * Assign socket to be part of the IPv4 internet family.
	 * Assign the port to the socket
	 */
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	/* [C3]
	 * Explaint the following here.
	 * inet_pton() to convert addresses(IPv4/IPv6) from text to binary form.
	 * It converts the character string "127.0.0.1" into a network address structure in the AF_INET family.
	 * It then copies the network address structure to sin_addr.
	 */
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	/* [C4]
	 * Explaint the following here.
	 * To initiate  a coonnection on a socket
	 * connects the socket "sock" to the address specified by "serv_addr"
	 * if connect() returns -1,print "Connection failed".
	 */
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed \n");
		return -1;
	}


	/* [C5]
	 * Explaint the following here.
	 * Get an input from user to continue the communication between server and client.
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [C6]
	 * Explaint the following here.
	 * Respond to Server.
	 * data from the socket.Here "sock" is the file descriptor of the sending socket.
	 * server transmits "Hello from client" to the Server.
	 */
	send(sock , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");

	/* [C7]
	 * Explaint the following here.
	 * Read from the Server and store it in buffer.
	 * read(int fd,void *buf,size_t count) reads up to count bytes from file descriptor into buf
	 * if read() return "-1",print error and exit. 
	 * Print the buffer for server's message.
	 */
	if (read( sock , buffer, 1024) < 0) {
		printf("\nRead Failed \n");
		return -1;
    }
	printf("Message from a server: %s\n",buffer );
	return 0;
}
