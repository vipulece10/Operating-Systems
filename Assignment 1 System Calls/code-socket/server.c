#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFF_SIZE] = {0};
	char *hello = "Hello from server";

	/* [S1]
	 * Explaint the following here
	 * socket(Address Family,Type,Protocol) To create a socket and return a descriptor which can be used further
	 * Address Family - AF_INET (this is IP version 4) Type - SOCK_STREAM (this means connection oriented TCP protocol) 
	 * Protocol - 0 [ or IPPROTO_IP This is IP protocol] 
	 * if socket() return "-1",print error and exit.
	 * included in sys/socket.h header 
	 */
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	/* [S2]
	 * Explaint the following here.
	 * To set options at socket level for socket created above(server_fd)
	 * SOL_SOCKET specifies the protocol level.
	 * SO_REUSEADDR specifies that the rules while validating addresses in bind() call should allow reuse of local address.
	 * setsockopt() will set the option specified to the value pointed by the option_value argument(&opt) for the socket.
	 * if socket() return "-1",print error and exit.
	 * included in sys/socket.h header 
	 */
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
		       &opt, sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	/* [S3]
	 * Set Port and IP.
     * server will be bind to the local host hence s.addr attribute of address(custom data structure) is set to INADDR_ANY for IP address
	 */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	/* [S4]
	 * bind to the set port and IP.
	 * bind() is used to associate the socket with local address i.e. IP Address,port and address family set above
	 */
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	/* [S5]
	 * To put the server in listening mode.
	 * listen() marks a socket specified by socket argument server_fd as accepting connections.
	 * In listen(int s,int backlog),backlog argument limits the number of outstanding connections in the socket's listen queue to the its value
	 * if socket() return "-1",print error and exit. 
	 */
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* [S6]
	 * Accept an incoming connection
	 * to get the first connection request on the queue of pending connections for the listening socket, sockfd, 
	 * creates a new connected socket, and returns a new file descriptor(new_socket) referring to that socket. 
	 */
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
				 (socklen_t*)&addrlen)) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	/* [S7]
	 * Get an input from user to continue the communication between server and client
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [S8]
	 * Read from the client and store it in buffer.
	 * read(int fd,void *buf,size_t count) reads up to count bytes from file descriptor into buf
	 * if read() return "-1",print error and exit. 
	 * Print the buffer for client's message.
	 */
	if (read( new_socket , buffer, 1024) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	printf("Message from a client: %s\n",buffer );

	/* [S9]
	 * Respond to client.
	 * send() transmits data from the socket.Here "new_socket" is the file descriptor of the sending socket.
	 * send() is mostly used when the socket is in a connected state.
	 * server transmits "Hello from Server" to the client.
	 */
	send(new_socket , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");
	return 0;
}
