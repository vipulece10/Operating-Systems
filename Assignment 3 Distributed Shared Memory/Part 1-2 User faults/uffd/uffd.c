/* userfaultfd_demo.c

   Licensed under the GNU General Public License version 2 or later.
*/
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>

(#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE);	\
	} while (0)

static int page_size;)

static void *
fault_handler_thread(void *arg)
{
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	static int fault_cnt = 0;     /* Number of faults so far handled */
	long uffd;                    /* userfaultfd file descriptor */
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;

	uffd = (long) arg;

	/* [H1]
	 * Explain following in here.
	 * In this section we are mapping a region of page-size to a disk which will be a faulting region here
	 * mmap function prototype : void * mmap (void *address, size_t length, int protect, int flags, int filedes,off_t offset)
	 * address argument specifies the starting address of the allocation and if the value of address is passed as NULL the kernel choses the starting addr
	 * length argument specifies the length of allocation (bytes) and should be greater than 0.
	 * protect argument specifies the protection level:(below flags can be concatenated with bitwise OR operator)
	 * PROT_READ specifies that pages may be read.
	 * PROT_WRITE specifies that pages may be write.
	 * flag argument is used for whether updates made in mapping will be visible to the other process or not.Below flags are used here
	 * MAP_SHARED : In this case mappings will be shared.Updates to the mappings are visible to other processes that maps the same region.
	 * MAP_PRIVATE: In this case  If MAP_PRIVATE is specified, modifications to the mapped data by the calling process will be visible only to the calling process and will not change the underlying object.
	 * filedes argument specifies the file descriptor which has to be mapped
	 * offset argument specifies the offset from where the mapping started. 
	 * On failure mmap() returns MAP_FAILED.
	 * Exit with "mmap" message,in case mmap() returns MAP_FAILED on failure.
	 */
	if (page == NULL) {
		page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page == MAP_FAILED)
			errExit("mmap");
	}

	/* [H2]
	 * Explain following in here.
	 * this will be continuously executed as it is under infinite loop.	
	 * This section will keep on polling the userfaultfd file desciptor to handle the page faults if any.
	 */
	for (;;) {

		/* See what poll() tells us about the userfaultfd */

		struct pollfd pollfd;
		int nready;

		/* [H3]
		 * Explain following in here.
		 * pollfd struct defined in #include <poll.h> contains following members:
		 * pollfd.fd: The descriptor being polled (uffd file descriptor in this case)
		 * pollfd.events: The input event flags(POLLIN in this case for Data other than high priority data which can be read without blocking).
		 * pollfd.revents : The output event flags.
		 * poll() : userspace function which provides application to monitor I/O events on a set of file descriptors
		 * function prototype int poll(struct pollfd *fds,unsigned int nfds,int timeout)
		 * second argument nfds specifies the number of file descriptors on which polling is being done
		 * third argument specifies that the poll function will wait for this many milliseconds for the event to occur.
		 * if the value of third arg is 0 then the function return immediately.
		 * if the value of third arg is -1 then poll shall block untill a requested event has ocurred or an interrupt is ocurred
		 * On successful return ,poll() returns a positive integer in nready
		 * Upon failure print "poll" as error message and exit. 
		 * POLLIN and POLLERR were the bits returned in revents field.
		 */
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1)
			errExit("poll");

		printf("\nfault_handler_thread():\n");
		printf("    poll() returns: nready = %d; "
                       "POLLIN = %d; POLLERR = %d\n", nready,
                       (pollfd.revents & POLLIN) != 0,
                       (pollfd.revents & POLLERR) != 0);

		/* [H4]
		 * Explain following in here.
		 * to read an event from uffd file descriptor
		 * defined in #include <unistd.h>
		 * function prototype ssize_t read(int fd,void *buf,size_t count)
		 * read upto sizeof(msg) bytes from file descriptor uffd into the buffer string starting at &msg
		 * On success,the number of read bytes is returned
		 * read() returns zero if no bytes are read or the file offset is at or after the end of file.
		 * read () returns -1 on error.
		 * print "EOF on userfaultfd" and exit if nread return 0;
		 * exit printing "read" if read() returns -1;
		 */
		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
			exit(EXIT_FAILURE);
		}

		if (nread == -1)
			errExit("read");

		/* [H5]
		 * Explain following in here.
		 * We are concerned only for pagefault event from uffd file descriptor.
		 * If the event description is different from page_fault event print msg "Unexpected event on uffd" and exit
		 */
		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		/* [H6]
		 * Explain following in here.
		 * print the details about the page-fault event.
		 * flags : attribute of pagefault struct that is used to describe fault
		 * address : attribute of pagefault struct specifying the address that triggered the page fault.
		 */
		printf("    UFFD_EVENT_PAGEFAULT event: ");
		printf("flags = %llx; ", msg.arg.pagefault.flags);
		printf("address = %llx\n", msg.arg.pagefault.address);

		/* [H7]
		 * Explain following in here.
		 * Copy the character "'A'+fault_cnt % 20" to the address pointed by page upto the size specified(page_size)
		 * change the fault_cnt so that the copied contents can be varied which tells us that each fault is handled separately. 
		 */
		memset(page, 'A' + fault_cnt % 20, page_size);
		fault_cnt++;

		/* [H8]
		 * Explain following in here.
		 * Setting the below fields of uffdio_copy so that the struct can be used in ioctl operation to copy a continuous memory chunk \ 
		 * into the memory range registered by userfault and wake up the blocked thread.
		 * uffdio_copy.src = souce adress from where copying is done (address pointed by page)
		 * uffdio_copy.dst = dest of copying operation (msg.arg.pagefault.address & ~(page_size - 1)) \
		 * msg.arg.pagefault.address &	~(page_size - 1); [Rounding address to page boundary as we will handle page faults for each page]
		 * uffdio_copy.len  (Number of bytes to copy) In our case this is specified to page_size
		 * uffdio_copy.mode (flag to control behaviour of the copy)
		 * uffdio_copy.copy (Number of bytes copied or negated error)
		 */
		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
			~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;

		/* [H9]
		 * Explain following in here.
		 * copying a page of data into the faulting region using the UFFDIO_COPY ioctl(2) operation.
                 * ioctl call takes three parameters file descriptor,struct and add of struct.
                 * defined in #include<sys/ioctl.h>
		 */
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

		/* [H10]
		 * Explain following in here.
		 * Printing the length of bytes that were copied.
		 * copy field of struct uffdio_copy stores the length of copied bytes  
		 */
		printf("        (uffdio_copy.copy returned %lld)\n",
                       uffdio_copy.copy);
	}
}

int
main(int argc, char *argv[])
{
	long uffd;          /* userfaultfd file descriptor */
	char *addr;         /* Start of region handled by userfaultfd */
	unsigned long len;  /* Length of region handled by userfaultfd */
	pthread_t thr;      /* ID of thread that handles page faults */
	struct uffdio_api uffdio_api;
	struct uffdio_register uffdio_register;
	int s;
	int l;

	/* [M1]
	 * Explain following in here.
	 * If the user doesn't provide the correct number of command line arguments show him/her correct usage guideline and exit. 
	 */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s num-pages\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* [M2]
	 * Explain following in here.
	 * calculate the size needs to be allcated based on the number of pages provided as argument 
	 * sysconf(_SC_PAGE_SIZE) returns the size of a page in bytes
	 */
	page_size = sysconf(_SC_PAGE_SIZE);
	len = strtoul(argv[1], NULL, 0) * page_size;

	/* [M3]
	 * Explain following in here.
	 * Create and enable userfaultfd object
	 * function prototype : int syscall(SYS_userfaultfd ,int flags)
	 * userfaultfd - create a file descriptor for handling page faults in user space.
	 * While creating userfaultfd() below flags are provided:
	 * O_CLOEXEC : It enables the close on exec flag for the new userfaultfd file decriptor. 
	 * O_NONBLOCK : It permits the use of file descriptor in a non blocking way.
	 * If the syscall returns -1 (on failure) ,print "userfaultfd" and exit.
	 */
	uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd == -1)
		errExit("userfaultfd");

	/* [M4]
	 * Explain following in here.
	 * userfaultfd must be enabled invoking the UFFDIO_API ioctl specifying a uffdio_api.api value set to UFFD_API
	 * uffdio_api.api value set to UFFD_API will specify the read/POLLIN protocol userland intends to speak on the UFFD
 	 *	uffdio_api.features will specify the features user-land application requires  
	 */
	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
		errExit("ioctl-UFFDIO_API");

	/* [M5]
	 * Explain following in here.
	 * In this section we are creating a private anonymous mapping in the virtual address space of this process.
	 * This memory is not yet allocated.When the thread tries to read/write this memory,it will be allocated by the userfaultfd.
	 * mmap function prototype : void * mmap (void *address, size_t length, int protect, int flags, int filedes,off_t offset)
	 * address argument specifies the starting address of the allocation and if the value of address is passed as NULL the kernel choses the starting addr
	 * length argument specifies the length of allocation (bytes) and should be greater than 0.
	 * protect argument specifies the protection level:(below flags can be concatenated with bitwise OR operator)
	 * PROT_READ specifies that pages may be read.
	 * PROT_WRITE specifies that pages may be write.
	 * flag argument is used for whether updates made in mapping will be visible to the other process or not.Below flags are used here
	 * MAP_SHARED : In this case mappings will be shared.Updates to the mappings are visible to other processes that maps the same region.
	 * MAP_PRIVATE: In this case  If MAP_PRIVATE is specified, modifications to the mapped data by the calling process will be visible only to the calling process and will not change the underlying object.
	 * filedes argument specifies the file descriptor which has to be mapped
	 * offset argument specifies the offset from where the mapping started. 
	 * On failure mmap() returns MAP_FAILED.
	 * Exit with "mmap" message,in case mmap() returns MAP_FAILED on failure.
	 * On success,mmap() returns a pointer to the mapped area.Print the address returned by mmap
	 */
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		errExit("mmap");

	printf("Address returned by mmap() = %p\n", addr);

	/* [M6]
	 * Explain following in here.
	 * Here we are registering the memory of len(uffdio_register.range.len) with start address(uffdio_register.range.start) to the   \ 
	 	userfaultfd object using ioctl system call.
	 * The mode field (uffdio_register.mode) defines the mode of operation desired for this
      memory region.
	 * UFFDIO_REGISTER_MODE_MISSING is used to track page faults on missing pages.
	 * On failure ioctl() system call returns -1.
	 * Exit with "ioctl-UFFDIO_REGISTER" message,in case ioctl() system call  returns -1.
	 */
	uffdio_register.range.start = (unsigned long) addr;
	uffdio_register.range.len = len;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
		errExit("ioctl-UFFDIO_REGISTER");

	/* [M7]
	 * Explain following in here.
	 * Create a thread to process the userfaultfd events.
	 * The new thread starts execution by invoking fault_handler_thread(); uffd is passed as the sole argument of    \
         * fault_handler_thread() to continuously poll the file descriptor(uffd) in background and check for page faults.
         * on success, pthread_create() returns 0 else exit by printing "pthread_create".
	 */
	s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
	if (s != 0) {
		errno = s;
		errExit("pthread_create");
	}

	/*
	 * [U1]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * Here we are trying to access the memory range pointed by addr which in turn is returned by mmap using read operation.
	 * It will create a page fault on the first access of the page.The pafe fault event will be handled by userfaultfd of  fault_handler_thread().
	 * userfaultfd is handling the page fault event by setting 'fault_count' to the entire memory to keep track of number of page faults occured.
	 * Since this is the first page fault,entire memory will be set with 'A' by the userfaultfd.
	 * Here we are printing the contents of the entire page in a interval of 1024 bytes by incrementing the iterator 'l' by 1024 bytes each time.
	 * Since the page size is 4096,'A' will be printed four times. 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#1. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U2]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * Again we are trying to access the memory range pointed by addr.
	 * Since we are accessing this memory range for the second time with no allocation changes,this will not create a page fault \
	 * and memory content will remain unchanged.
	 * Here we are printing the contents of the entire page in a interval of 1024 bytes by incrementing the iterator 'l' by 1024 bytes each time.
	 * Since the page size is 4096,'A' will be printed four times. 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#2. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U3]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * madvise() system call instructs the kernel how it expects to use memory address range beginning at 'addr' with size 'len' bytes.
	 * MADV_DONTNEED argument in madvise() advises to the kernel for the address range that do not expect access in the near future  \
	 * which causes the kernel to free up the memory address specified by addr. 
	 * On success, madvise() returns zero.  Print "fail to madvise" and exit when madvise() doesn't returns zero.
	 * Here we are again trying to access the same memory range by read operation after freeing up its content which will trigger a page fault.
	 * userfaultfd is handling the page fault event by setting 'fault_count' to the entire memory to keep track of number of page faults occured.
	 * Since this is the second page fault,entire memory will be set with 'B' by the userfaultfd.
	 * Here we are printing the contents of the entire page in a interval of 1024 bytes by incrementing the iterator 'l' by 1024 bytes each time.
	 * Since the page size is 4096,'B' will be printed four times. 
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#3. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U4]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * reaccessing the memory range pointed by addr.
	 * Since we are accessing this memory range for the second time(after madvise()) with no allocation changes,this will not create a page fault \
	 * and memory content will remain unchanged.
	 * Here we are printing the contents of the entire page in a interval of 1024 bytes by incrementing the iterator 'l' by 1024 bytes each time.
	 * Since the page size is 4096,'B' will be printed four times. 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#4. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U5]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * madvise() system call instructs the kernel how it expects to use memory address range beginning at 'addr' with size 'len' bytes.
	 * MADV_DONTNEED argument in madvise() advises to the kernel for the address range that do not expect access in the near future  \
	 * which causes the kernel to free up the memory address specified by addr. 
	 * On success, madvise() returns zero.  Print "fail to madvise" and exit when madvise() doesn't returns zero.
	 * Here we are again trying to access the same memory range by write operation after freeing up its content which will trigger a page fault.
	 * userfaultfd is handling the page fault event by setting 'fault_count' to the entire memory to keep track of number of page faults occured.
	 * Since this is the second page fault,entire memory will be set with 'B' by the userfaultfd.
	 * Post userfault handling we are setting '@' to the entire memory using memset library function.
	 * Here we are printing the contents of the entire page in a interval of 1024 bytes by incrementing the iterator 'l' by 1024 bytes each time.
	 * Since the page size is 4096,'@' will be printed four times.
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		memset(addr+l, '@', 1024);
		printf("#5. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U6]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * reaccessing the memory range pointed by addr.
	 * Since we are accessing this memory range for the second time(after madvise()) with no allocation changes,this will not create a page fault \
	 * and memory content will remain unchanged.
	 * Here we are printing the contents of the entire page in a interval of 1024 bytes by incrementing the iterator 'l' by 1024 bytes each time.
	 * Since the page size is 4096,'@' will be printed four times. 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#6. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U7]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * madvise() system call instructs the kernel how it expects to use memory address range beginning at 'addr' with size 'len' bytes.
	 * MADV_DONTNEED argument in madvise() advises to the kernel for the address range that do not expect access in the near future  \
	 * which causes the kernel to free up the memory address specified by addr. 
	 * On success, madvise() returns zero.  Print "fail to madvise" and exit when madvise() doesn't returns zero.
	 * Here we are again trying to access the same memory range by write operation after freeing up its content which will trigger a page fault.
	 * userfaultfd is handling the page fault event by setting 'fault_count' to the entire memory to keep track of number of page faults occured.
	 * Since this is the second page fault,entire memory will be set with 'B' by the userfaultfd.
	 * Post userfault handling we are setting '^' to the entire memory using memset library function.
	 * Here we are printing the contents of the entire page in a interval of 1024 bytes by incrementing the iterator 'l' by 1024 bytes each time.
	 * Since the page size is 4096,'^' will be printed four times.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		memset(addr+l, '^', 1024);
		printf("#7. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U8]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 * reaccessing the memory range pointed by addr.
	 * Since we are accessing this memory range for the second time(after madvise()) with no allocation changes,this will not create a page fault \
	 * and memory content will remain unchanged.
	 * Here we are printing the contents of the entire page in a interval of 1024 bytes by incrementing the iterator 'l' by 1024 bytes each time.
	 * Since the page size is 4096,'^' will be printed four times. 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#8. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	exit(EXIT_SUCCESS);
}
