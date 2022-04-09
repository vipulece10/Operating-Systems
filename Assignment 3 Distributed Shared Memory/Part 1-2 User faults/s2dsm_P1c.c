#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

#define BUFF_SIZE 4096

//macro to print msg on error
#define errExit(msg)   do { perror(msg); exit(EXIT_FAILURE); \
} while (0);

static int page_size;

static inline void ignore_ret() {} 

//function to invoke fault_handler_thread with userfaultfd to habdle page fault events
static void *
fault_handler_thread(void *arg)
{
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	long uffd;                    /* userfaultfd file descriptor */
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;

	uffd = (long) arg;

	// to create a mapping in the virtual memory of length page_size
	if (page == NULL) {
		page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page == MAP_FAILED)
			errExit("mmap");
	}

	// infinite loop to continuously poll userfault file descriptor
	for (;;) {

		struct pollfd pollfd;
		int nready;

		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1)
			errExit("poll");


		printf(" [x] PAGEFAULT\n");

		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
			exit(EXIT_FAILURE);
		}

		if (nread == -1)
			errExit("read");

		
		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		//setting the entire content of page with '\0'
		memset(page, '\0', page_size);
		

		
		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
		~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;

		
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

	}
}


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
		printf("Connected Created successfully... \n");
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

	// Changes Added for Part1c for registering the memory region with userfaultfd

	long uffd;          /* userfaultfd file descriptor */
	pthread_t thr;      /* ID of thread that handles page faults */
	struct uffdio_api uffdio_api;
	struct uffdio_register uffdio_register;
	char operation;
	int page_num;
	int s;
	int i;
	char msg[page_size-1];
	char *temp;
	char read_buffer[page_size];
	
	uffd = syscall( __NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd == -1)
		errExit("userfaultfd");

	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
		errExit("ioctl-UFFDIO_API");


	uffdio_register.range.start = (unsigned long) mapped_address;
	uffdio_register.range.len = (unsigned long) alloc_size;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
		errExit("ioctl-UFFDIO_REGISTER");

	s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
	if (s != 0) {
		errno = s;
		errExit("pthread_create");
	}
	while(1){
		// Ask for user input once paired
		printf("> Which command should I run? (r:read, w:write): ");
		ignore_ret(scanf(" %c",&operation));

		printf("> For which page? (0-%d, or -1 for all): ",(pages-1));
		ignore_ret(scanf("%d",&page_num));

		switch(operation){
			case 'r' :;
			//read the contents of the page requested
			i=(page_num==-1)?0:page_num;
			while(i < pages){
				sprintf(read_buffer,"%s",mapped_address+(i*page_size));
				printf(" [*] Page %i \n%s\n",i,read_buffer);
				if(page_num != -1)
					break;
				i++;
			}

			break;

			case 'w' :;

			int msglen;
			int j;		
			msglen=0;
			printf("> Type your message \n");
			while(fgets(msg,page_size,stdin) != NULL){
				msglen=strlen(msg);
				if(msglen > 1){
					if (msg[msglen-1]=='\n'){
						msg[msglen-1]='\0';
						break;
					}
					else{
						printf("Buffer limit exceeded.String till page size (4096) will be stored\n");
						break;
					}
				}
			}
                
			for(j=(page_num==-1)?0:page_num;j<pages;j++){
				temp=mapped_address + j*page_size;
				memcpy(temp,msg,page_size);
				if(page_num != -1)
					break;
			}


			int k;
			k=(page_num==-1)?0:page_num;
			while(k < pages){
				printf(" [*] Page %i:\n%s\n",k,mapped_address+k*page_size);
				if(page_num != -1)
					break;
				k++;
			}

			break;

			default:
			printf("Invalid Input\n");

		}
	}



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
