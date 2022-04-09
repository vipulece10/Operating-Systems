#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/mman.h>


#define BUFF_SIZE 4096

//macro to print msg on error
#define errExit(msg)   do { perror(msg); exit(EXIT_FAILURE); \
} while (0);

static int page_size;

static inline void ignore_ret() {} 

int main(int argc, const char *argv[])
{
	int server_fd, new_socket,sock;
	struct sockaddr_in address,serv_addr;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFF_SIZE] = {0};
	char *mapped_address=NULL;
	int pages;
	unsigned long alloc_size=0;
	int port_local,port_remote;
	int isFirstProcess=0;

	if(argc < 3){
		fprintf(stderr,"ERROR,provide both local and remote port\n");
		exit(1);
	}

	port_local=atoi(argv[1]);
	port_remote=atoi(argv[2]);


	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror(" \n Server Socket creation error \n");  
		exit(EXIT_FAILURE);
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Client Socket creation error \n");
		return -1;
	} 


	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
		&opt, sizeof(opt))) {
		perror("setsockopt\n");
	exit(EXIT_FAILURE);
}

address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
address.sin_port = htons( port_local );

if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
	perror("Binding failed\n");
	exit(EXIT_FAILURE);
}   

memset(&serv_addr, '0', sizeof(serv_addr));
serv_addr.sin_family = AF_INET;
serv_addr.sin_port = htons(port_remote);

if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
	printf("\nInvalid address/ Address not supported \n");
	return -1;
}


if (listen(server_fd, 3) < 0) {
	perror("listen");
	exit(EXIT_FAILURE);
}

printf("Local port is ready to accept connection \n");

if(!isFirstProcess){
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("Connection Failed.Connection will be reattempted... \n");
		isFirstProcess++;
	}
	else{
		printf("Connected created successfully. \n");
	}
}



if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
	(socklen_t*)&addrlen)) < 0) {
	perror("accept\n");

}

printf("Connection accepted from remote port\n");

if(isFirstProcess){
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed Again... \n");
	}
	else{
		printf("Connected created successfully after reattempt... \n");
	}
}
page_size = sysconf(_SC_PAGE_SIZE);

if(isFirstProcess){
	printf("> How many pages would you like to allocate (greater than 0)? \n");
	ignore_ret(scanf("%d",&pages));
	alloc_size=pages*page_size;
	if(mapped_address==NULL){
		mapped_address=mmap(NULL,alloc_size,PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		if(mapped_address==MAP_FAILED)
			printf("Error in mmap \n");
		
	}
	printf("mmapped size: %lu \n",alloc_size);
	printf("Address of the mapped region: %p\n",mapped_address);
	sprintf(buffer,"%p  %lu",mapped_address,alloc_size);
	send(sock,buffer,strlen(buffer),0);	
}else{
	if(read(new_socket,buffer,1024)<0){
		printf("Read failed\n");
	}
	void *address;
	char *token;
	char *ptr;
	long len;
	token = strtok(buffer," ");
	sscanf(token,"%p",&address);
	printf("Address of mmapped region received from first process: %p\n",address);
	token=strtok(NULL," ");

	
	len=strtoul(token,&ptr,10);
	
	printf("mmapped size received from first process: %lu\n",len);

	char *remapped_address=mmap(address,len,PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
	if(remapped_address==MAP_FAILED)
		printf("Error in Mapping\n");
	printf("Address of re-mmapped region: %p\n",remapped_address);
	printf("re-mmapped size: %lu \n",len);

}
return 0;
}
