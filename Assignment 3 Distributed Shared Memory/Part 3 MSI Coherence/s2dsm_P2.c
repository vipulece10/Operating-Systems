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
static int pages, sock, new_socket; 
static char *initial_address;

// Declare an enum to store msi state
enum msi_state{M,S,I};
//Declare an msi array to keep track of msi information and initially all pages are in invalid state
 // static enum msi_state msi_array[100];
static enum msi_state *msi_array;
void updatePagestatus(int page);
void writeToPage(char *address,int page,const char *msg);

static inline void ignore_ret() {} 

char msistate_toString(enum msi_state val){
	if (val == I)
		return 'I';
	else if(val == M)
		return 'M';
	else 
		return 'S';
}

static void *second_process_response(void *arg){
	char buffer[BUFF_SIZE];
	int s;
	
	for(;;){
		memset(buffer,'\0',page_size);
		s=read(new_socket,buffer,BUFF_SIZE);

		if( s < 0){
			errExit("Error in reading");
		}
		else if(s > 0){
			char msg[BUFF_SIZE];
			char *token;
			char *operation_requested;
			char *temp;
			token=strtok(buffer," ");
			int page_num=atoi(token);
			token=strtok(NULL," ");
			operation_requested=token;
			enum msi_state st=msi_array[page_num];
			if(strcmp(operation_requested,"Invalidate")==0){
				msi_array[page_num]=I;
				char *page_address=initial_address+(page_num*page_size);
				if(madvise(page_address,page_size,MADV_DONTNEED)){
					errExit("Fail to madvise");
				}
				sprintf(msg,"Page Invalidated successfully.");
				printf("\nPage %d : Page state was %c. \n",page_num,msistate_toString(st));
				printf("Page %d : Received Invalidate Request from first process.%s.\n",page_num,msg);
			} 
			else if(strcmp(operation_requested,"Read")==0){
				if(st == I){
					msi_array[page_num]=S;
					temp=initial_address+(page_num * page_size);
					snprintf(msg,page_size,"%s",temp);
					if(msg[0]=='\0'){
					strcpy(msg,"NULL");
					}
				}
				else if(st == M){
					msi_array[page_num]=S;
					temp=initial_address+(page_num * page_size);
					snprintf(msg,page_size,"%s",temp);
				}
				else{
					temp=initial_address+(page_num*page_size);
					snprintf(msg,page_size,"%s",temp);
					if(msg[0]=='\0'){
					strcpy(msg,"NULL");
					}	
				}
				printf("\nPage %d : Page state was %c. \n",page_num,msistate_toString(st));
				printf("Page %d : Received Read Request from first process.Sent content of page : %s to first process.\n",page_num,msg);
			}
			send(new_socket,msg,strlen(msg),0);
		}
	}
}


void writeToPage(char *address,int page,const char *msg){
	int j;
	char *temp;
	for(j=(page == -1)?0:page;j<pages;j++){
		temp=address + j*page_size;
		memcpy(temp,msg,page_size);
		if(page != -1)
			break;
	}
}

//fetching from second process
char *fetch_second_process(int page ,char *operation){
	static char tmp_msg[BUFF_SIZE];
	char * res;
	sprintf(tmp_msg,"%d %s",page,operation);
	send(sock,tmp_msg,strlen(tmp_msg),0);
	int t;
	memset(tmp_msg,'\0',BUFF_SIZE);
	t=read(sock,tmp_msg,page_size);
	if(t < 0){
		errExit("Fail to read");
	}
	else if(t == 0){
		printf("Nothing to read \n");
		tmp_msg[0]='\0';
	}
	res=tmp_msg;
	return res;
}

//update page status on write
void updatePageStatus(int page){
		msi_array[page] = M;
		printf("\nPage %d : Writing completed.Changing page status to M,Sent Invalidate request to second process. \n",page);
		fetch_second_process(page,"Invalidate");
	
}

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
	int server_fd;
	struct sockaddr_in address,serv_addr;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFF_SIZE] = {0};
	char *mapped_address=NULL;
	
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
	snprintf(buffer,BUFF_SIZE,"%p  %lu",mapped_address,alloc_size);
	
	send(sock,buffer,strlen(buffer),0);	
}else{
	if(read(new_socket,buffer,BUFF_SIZE)<0){
		printf("Read failed\n");
	}
	
	char *token;
	char *ptr;
	token = strtok(buffer," ");
	sscanf(token,"%p",&mapped_address);
	
	printf("Address of mmapped region received from first process: %p\n",mapped_address);

	token=strtok(NULL," ");

	alloc_size=strtoul(token,&ptr,10);

	pages=alloc_size/page_size;

	printf("mmapped size received from first process: %lu\n",alloc_size);

	mapped_address=mmap(mapped_address,alloc_size,PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);

	if(mapped_address==MAP_FAILED)
		printf("Error in Mapping\n");
	
	
	printf("Address of re-mmapped region: %p\n",mapped_address);
	printf("re-mmapped size: %lu \n",alloc_size);		

}
initial_address=mapped_address;
// Changes Added for Part1c for registering the memory region with userfaultfd

long uffd;          /* userfaultfd file descriptor */
pthread_t thr,msi;      /* ID of thread that handles page faults */
struct uffdio_api uffdio_api;
struct uffdio_register uffdio_register;
char operation;
int page_num;
int s,t;
int i;
char msg[page_size-1];
char *temp;
	

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

msi_array=malloc(pages * sizeof(int));
int ii;
for(ii=0;ii<pages;ii++){
	msi_array[ii]=I;
}

	//create a thread which will work coherently with first process to enforce MSI protocol.

t = pthread_create(&msi, NULL, second_process_response, (void *) uffd);
if (t != 0) {
	errno = t;
	errExit("pthread_create");
}


while(1){
		// Ask for user input once paired
	printf("> Which command should I run? (r:read, w:write,v:view msi array): \n");
	ignore_ret(scanf(" %c",&operation));
		

	if(operation !='v'){
		printf("> For which page? (0-%d, or -1 for all): \n",(pages-1));
		ignore_ret(scanf("%d",&page_num));
		
	}
	
	switch(operation){

		case 'r' :;
			//read the contents of the page requested
		char page_content[BUFF_SIZE];
		char *mesg;
		i=(page_num==-1)?0:page_num;
		while(i < pages){
			sprintf(page_content,"%s",initial_address+(i*page_size));
			enum msi_state st=msi_array[i];
			if(st == I){
				mesg=fetch_second_process(i,"Read");
				if(strcmp("NULL",mesg)!=0){
					writeToPage(initial_address,i,mesg);
					msi_array[i]=S;
				}
				else{	
					msi_array[i]=S;
					mesg="";
				}
				sprintf(page_content,"%s",mesg);
			}
			printf(" [*] Page %i \n%s\n",i,page_content);
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
			updatePageStatus(j);
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

		case 'v' :;
		int i;
		for(i=0;i<pages;i++){
			enum msi_state state = msi_array[i];
			if (state == I)
				printf(" [*] Page %d : %s\n",i,"I");
			else if(state == M)
				printf(" [*] Page %d : %s\n",i,"M");
			else 
				printf(" [*] Page %d : %s\n",i,"S");
		}

		break;

		default:
		printf("Invalid Input\n");

	}
}
return 0;
}
