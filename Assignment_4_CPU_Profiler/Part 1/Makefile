
obj-m += perftop.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

install:
	sudo insmod perftop.ko

uninstall:
	sudo rmmod perftop.ko

