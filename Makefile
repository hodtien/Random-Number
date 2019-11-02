KDIR = /lib/modules/`uname -r`/build

all:
	make -C $(KDIR) M=`pwd` modules
	cc -o test test.c

clean:
	make -C $(KDIR) M=`pwd` clean
	rm test
