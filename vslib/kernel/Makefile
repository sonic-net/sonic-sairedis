# Minimal kernel module for SONiC Stream Telemetry (virtual SAI)
# Registers genetlink family "sonic_stel" with multicast group "ipfix"

obj-m := sonic_stel.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

# DKMS support
dkms-install:
	mkdir -p /usr/src/sonic-stel-1.0
	cp sonic_stel.c Makefile dkms.conf /usr/src/sonic-stel-1.0/
	dkms add -m sonic-stel -v 1.0
	dkms build -m sonic-stel -v 1.0
	dkms install -m sonic-stel -v 1.0

dkms-remove:
	dkms remove -m sonic-stel -v 1.0 --all
	rm -rf /usr/src/sonic-stel-1.0

.PHONY: all clean install dkms-install dkms-remove
