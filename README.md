# DS1302 linux driver via GPIO

## Devices

1. RTC (/dev/rtcX)
2. static RAM (/sys/bus/nvmem/devices/ds1302-0/nvmem)

## Usage
1. Modify device tree

Add following node to your device tree, be aware to modify the GPIOs to fit your needed.
```dts
playos_ds1302@0 {
    compatible = "playos,ds1302";
    ce-gpio = <&gpio GPIOH_8 GPIO_ACTIVE_HIGH>;
    clk-gpio = <&gpio GPIOH_9 GPIO_ACTIVE_HIGH>;
    data-gpio = <&gpio_ao GPIOAO_6 GPIO_ACTIVE_HIGH>;
    burst-mode;

    status = "okay";
};

```

2. Setup environment variable
i.e:
```bash
# your kernel source dir
export KDIR=/lib/modules/`uname -r`/build
# your cross compiler
export CROSS_COMPILE=aarch64-linux-gnu-
# the target cpu architecture
export ARCH=arm64
```

3. Compiling

Execute `make` in source directory to compiling.


4. Then

Update your device tree of your device, copy the `playos_ds1302.ko` file to your deivce, and load the driver.
