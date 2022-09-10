#include <asm/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/bcd.h>


#define RTC_WRITE   0x00
#define RTC_READ    0x01

#define RTC_SECOND_ADDR    0x80
#define RTC_MINUTE_ADDR    0x82
#define RTC_HOUR_ADDR      0x84
#define RTC_DATE_ADDR      0x86
#define RTC_MONTH_ADDR     0x88
#define RTC_DAY_ADDR       0x8A
#define RTC_YEAR_ADDR      0x8C
#define RTC_WP_ADDR        0x8E
#define RTC_TCS_ADDR       0x90

#define RTC_CLOCK_BURST 0xBE
#define RTC_RAM_BURST   0xFE

#define ds1302_delay(n) ndelay(n)


struct playos_ds1302_device {
    struct platform_device *pdev;

    struct rtc_device *rtc;

    struct gpio_desc *ce;
    struct gpio_desc *clk;
    struct gpio_desc *data;

    bool burst_mode;
};

static int playos_ds1302_read_buffer(struct playos_ds1302_device *dev,
        uint8_t addr, uint8_t *data, size_t size)
{
    int i, j;

    gpiod_direction_output(dev->data, 0);
    gpiod_set_value(dev->clk, 0);
    gpiod_set_value(dev->ce, 1);
    ds1302_delay(1000);

    addr = addr | RTC_READ;

    for (i = 0; i < 8; ++i) {
        gpiod_set_value(dev->data, (addr & (0x01 << i)));
        ds1302_delay(50);
        gpiod_set_value(dev->clk, 1);
        ds1302_delay(70);
        gpiod_set_value(dev->clk, 0);
        ds1302_delay(250);
    }

    gpiod_direction_input(dev->data);
    for (j = 0; j < size; ++j) {
        data[j] = 0x00;
        for (i = 0; i < 8; ++i) {
            gpiod_set_value(dev->clk, 0);
            ds1302_delay(100);
            gpiod_set_value(dev->clk, 1);
            data[j] = data[j] | (gpiod_get_value(dev->data) << i);
        }
    }
    gpiod_set_value(dev->ce, 0);
    gpiod_set_value(dev->clk, 0);

    return 0;
}

static int playos_ds1302_write_buffer(struct playos_ds1302_device *dev,
        uint8_t addr, uint8_t *buffer, size_t size)
{
    int i, j;

    gpiod_direction_output(dev->data, 0);
    gpiod_set_value(dev->clk, 0);
    gpiod_set_value(dev->ce, 1);
    ds1302_delay(1000);

    addr = addr | RTC_WRITE;

    for (i = 0; i < 8; ++i) {
        gpiod_set_value(dev->data, (addr & (0x01 << i)));
        ds1302_delay(50);
        gpiod_set_value(dev->clk, 1);
        ds1302_delay(70);
        gpiod_set_value(dev->clk, 0);
        ds1302_delay(250);
    }

    for (j = 0; j < size; ++j) {
        dev_dbg(&dev->pdev->dev, "playos_ds1302_write, addr: %x, data: %x\n", addr, buffer[j]);
        for (i = 0; i < 8; ++i) {
            gpiod_set_value(dev->data, (buffer[j] & (0x01 << i)));
            ds1302_delay(50);
            gpiod_set_value(dev->clk, 1);
            ds1302_delay(70);
            gpiod_set_value(dev->clk, 0);
            ds1302_delay(250);
        }
    }
    gpiod_set_value(dev->ce, 0);
    gpiod_set_value(dev->clk, 0);

    return 0;
}

static int playos_ds1302_read(struct playos_ds1302_device *dev, uint8_t addr, uint8_t *data)
{
    return playos_ds1302_read_buffer(dev, addr, data, 1);
}

static int playos_ds1302_write(struct playos_ds1302_device *dev, uint8_t addr, uint8_t data)
{
    return playos_ds1302_write_buffer(dev, addr, &data, 1);
}


static int playos_ds1302_read_time(struct device *dev, struct rtc_time *tm)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct playos_ds1302_device *pddev = platform_get_drvdata(pdev);
    uint8_t buffer[8] = { 0 };

    if (pddev->burst_mode) {
        playos_ds1302_read_buffer(pddev, RTC_CLOCK_BURST, buffer, sizeof(buffer));
    } else {
        playos_ds1302_read(pddev, RTC_SECOND_ADDR, &buffer[0]);
        playos_ds1302_read(pddev, RTC_MINUTE_ADDR, &buffer[1]);
        playos_ds1302_read(pddev, RTC_HOUR_ADDR, &buffer[2]);
        playos_ds1302_read(pddev, RTC_DATE_ADDR, &buffer[3]);
        playos_ds1302_read(pddev, RTC_MONTH_ADDR, &buffer[4]);
        playos_ds1302_read(pddev, RTC_DAY_ADDR, &buffer[5]);
        playos_ds1302_read(pddev, RTC_YEAR_ADDR, &buffer[6]);
    }

    dev_dbg(dev, "raw data is sec=%02x, min=%02x, hr=%02x, "
        "mday=%02x, mon=%02x, wday=%02x, year=%02x\n",
        buffer[0], buffer[1], buffer[2],
        buffer[3], buffer[4], buffer[5], buffer[6]);

    tm->tm_sec = bcd2bin(buffer[0]);
    tm->tm_min = bcd2bin(buffer[1]);
    tm->tm_hour = bcd2bin(buffer[2]);
    tm->tm_mday = bcd2bin(buffer[3]);
    tm->tm_mon = bcd2bin(buffer[4]) - 1;
    tm->tm_wday = buffer[5] - 1;
    tm->tm_year = bcd2bin(buffer[6]) + 100;
    
    dev_dbg(dev, "tm is secs=%d, mins=%d, hours=%d, "
        "mday=%d, mon=%d, year=%d, wday=%d\n",
        tm->tm_sec, tm->tm_min, tm->tm_hour,
        tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday);

    return 0;
}

static int playos_ds1302_set_time(struct device *dev, struct rtc_time *tm)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct playos_ds1302_device *pddev = platform_get_drvdata(pdev);
    uint8_t buffer[8] = { 0 };

    if (pddev->burst_mode) {
        buffer[0] = bin2bcd(tm->tm_sec);
        buffer[1] = bin2bcd(tm->tm_min);
        buffer[2] = bin2bcd(tm->tm_hour);
        buffer[3] = bin2bcd(tm->tm_mday);
        buffer[4] = bin2bcd(tm->tm_mon + 1);
        buffer[5] = tm->tm_wday + 1;
        buffer[6] = bin2bcd(tm->tm_year - 100);
        buffer[7] = 0;

        playos_ds1302_write_buffer(pddev, RTC_CLOCK_BURST, buffer, sizeof(buffer));
    } else {
        playos_ds1302_write(pddev, RTC_SECOND_ADDR, bin2bcd(tm->tm_sec));
        playos_ds1302_write(pddev, RTC_MINUTE_ADDR, bin2bcd(tm->tm_min));
        playos_ds1302_write(pddev, RTC_HOUR_ADDR, bin2bcd(tm->tm_hour));
        playos_ds1302_write(pddev, RTC_DAY_ADDR, tm->tm_wday + 1);
        playos_ds1302_write(pddev, RTC_DATE_ADDR, bin2bcd(tm->tm_mday));
        playos_ds1302_write(pddev, RTC_MONTH_ADDR, bin2bcd(tm->tm_mon + 1));
        playos_ds1302_write(pddev, RTC_YEAR_ADDR, bin2bcd(tm->tm_year - 100));
    }

    return 0;
}

static const struct rtc_class_ops playos_ds1302_ops = {
    .read_time = playos_ds1302_read_time,
    .set_time = playos_ds1302_set_time,
};

static int playos_ds1302_probe(struct platform_device *pdev)
{
    int ret = 0;
    uint8_t data = 0x00;
    struct playos_ds1302_device *ds1302 = 
            devm_kzalloc(&pdev->dev, sizeof(struct playos_ds1302_device), GFP_KERNEL);
    if (ds1302 == NULL) {
        return -ENOMEM;
    }

    ds1302->pdev = pdev;
    platform_set_drvdata(pdev, ds1302);

    ds1302->burst_mode = of_property_read_bool(&pdev->dev.of_node[0], "burst-mode");
    dev_info(&pdev->dev, "DS1302 burst mode: %d\n", ds1302->burst_mode);
    ds1302->ce = devm_gpiod_get(&pdev->dev, "ce", GPIOD_OUT_LOW);
    if (IS_ERR(ds1302->ce)) {
        return PTR_ERR(ds1302->ce);
    }
    ds1302->clk = devm_gpiod_get(&pdev->dev, "clk", GPIOD_OUT_LOW);
    if (IS_ERR(ds1302->clk)) {
        return PTR_ERR(ds1302->clk);
    }
    ds1302->data = devm_gpiod_get(&pdev->dev, "data", GPIOD_OUT_LOW);
    if (IS_ERR(ds1302->data)) {
        return PTR_ERR(ds1302->data);
    }
    gpiod_direction_output(ds1302->clk, 0);
    gpiod_direction_output(ds1302->data, 0);
    gpiod_direction_output(ds1302->ce, 1);

    ds1302->rtc = devm_rtc_allocate_device(&pdev->dev);
    if (IS_ERR(ds1302->rtc)) {
        return PTR_ERR(ds1302->rtc);
    }

    ds1302->rtc->ops = &playos_ds1302_ops;
    ds1302->rtc->range_min = RTC_TIMESTAMP_BEGIN_2000;
    ds1302->rtc->range_max = RTC_TIMESTAMP_END_2099;
    ds1302->rtc->start_secs = 0;
    ds1302->rtc->set_start_time = true;

    playos_ds1302_read(ds1302, RTC_WP_ADDR, &data);
    if (data & 0x80) {
        // Enable clock write / disable write protected
        data = 0x00;
        playos_ds1302_write(ds1302, RTC_WP_ADDR, data);
    }

    ret = devm_rtc_register_device(ds1302->rtc);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static int playos_ds1302_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id playos_ds1302_of_ids[] = {
    { .compatible = "playos,ds1302" },
    {},
};
MODULE_DEVICE_TABLE(of, playos_ds1302_of_ids);

static struct platform_driver playos_ds1302_platform_driver = {
    .probe = playos_ds1302_probe,
    .remove = playos_ds1302_remove,
    .driver = {
        .name = "playos_ds1302",
        .of_match_table = playos_ds1302_of_ids,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(playos_ds1302_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rany@playos.xyz");
MODULE_DESCRIPTION("PlayOS DS1302-GPIO driver");
