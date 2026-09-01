obj-m += my-usb-driver.o


KERNEL_DIR=/lib/modules/$(shell uname -r)/build/

# Set these to the Priv and DER keys to sign modules. 
PRIVFILE="MOK.priv"
DERFILE="MOK.der"


#  If we were cross-compiling we would add: 	ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \  to each of these
all:
	make -C $(KERNEL_DIR) M=$(shell pwd) modules 
ifneq ($(PRIVFILE),"")
	/usr/src/linux-headers-$(shell uname -r)/scripts/sign-file sha256 \
	    $(PRIVFILE) \
	    $(DERFILE) \
	    ./my-usb-driver.ko 
endif 

clean:
	make -C $(KERNEL_DIR) M=$(shell pwd) clean 



