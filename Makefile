ifeq ($(KERNELRELEASE),)
export CONFIG_PLAYOS_DS1302_GPIO=m

KDIR ?= /lib/modules/`uname -r`/build
CROSS_COMPILE ?= aarch64-linux-gnu-
ARCH ?= arm64
CFLAGS = -I$(PWD)/include

ds1302:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$$PWD

sign: ds1302
	$(KDIR)/scripts/sign-file sha512 \
		$(KDIR)/certs/signing_key.pem \
		$(KDIR)/certs/signing_key.x509 \
		playos_ds1302.ko

clean:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$$PWD clean

endif

obj-$(CONFIG_PLAYOS_DS1302_GPIO) = playos_ds1302.o
