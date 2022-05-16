
obj-m += s2fs.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

install:
	sudo insmod s2fs.ko

uninstall:
	sudo rmmod s2fs.ko

