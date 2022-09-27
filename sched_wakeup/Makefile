obj-m += sched_profiler.o

.PHONY: hooks.h

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	chmod 777 sched_profiler.ko

hooks.h: create_hooks.pl
	   $(PWD)/create_hooks.pl $(PWD)

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf hooks.h

update:
	rmmod lps
	insmod lps.ko

