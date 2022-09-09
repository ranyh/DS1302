#include <asm/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/printk.h>

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

#define ds1302_delay(n) ndelay(n)


struct playos_ds1302_device {
    struct platform_device *pdev;

    struct rtc_device *rtc;

    struct gpio_desc *ce;
    struct gpio_desc *clk;
    struct gpio_desc *data;
};


static int playos_ds1302_read(struct playos_ds1302_device *dev, uint8_t addr, uint8_t *data)
{
    int i;

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
    *data = 0x00;
    for (i = 0; i < 8; ++i) {
        gpiod_set_value(dev->clk, 0);
        ds1302_delay(100);
        gpiod_set_value(dev->clk, 1);
        *data = (*data) | (gpiod_get_value(dev->data) << i);
    }

    return 0;
}

static int playos_ds1302_write(struct playos_ds1302_device *dev, uint8_t addr, uint8_t data)
{
    int i;

    gpiod_direction_output(dev->data, 0);
    gpiod_set_value(dev->clk, 0);
    gpiod_set_value(dev->ce, 1);
    ds1302_delay(1000);

    addr = addr | RTC_WRITE;

    dev_dbg("playos_ds1302_write, addr: %x, data: %x\n", addr, data);

    for (i = 0; i < 8; ++i) {
        gpiod_set_value(dev->data, (addr & (0x01 << i)));
        ds1302_delay(50);
        gpiod_set_value(dev->clk, 1);
        ds1302_delay(70);
        gpiod_set_value(dev->clk, 0);
        ds1302_delay(250);
    }

    for (i = 0; i < 8; ++i) {
        gpiod_set_value(dev->data, (data & (0x01 << i)));
        ds1302_delay(50);
        gpiod_set_value(dev->clk, 1);
        ds1302_delay(70);
        gpiod_set_value(dev->clk, 0);
        ds1302_delay(250);
    }

    return 0;
}


// static int playos_ds1302_ioctl(struct device *dev, unsigned int cmd, unsigned long value)
// {
//     return -ENOSYS;
// }

static int playos_ds1302_read_time(struct device *dev, struct rtc_time *tm)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct playos_ds1302_device *pddev = platform_get_drvdata(pdev);
    uint8_t seconds, minutes, hours, date, month, day, year;

    playos_ds1302_read(pddev, RTC_SECOND_ADDR, &seconds);
    playos_ds1302_read(pddev, RTC_MINUTE_ADDR, &minutes);
    playos_ds1302_read(pddev, RTC_HOUR_ADDR, &hours);
    playos_ds1302_read(pddev, RTC_DAY_ADDR, &day);
    playos_ds1302_read(pddev, RTC_DATE_ADDR, &date);
    playos_ds1302_read(pddev, RTC_MONTH_ADDR, &month);
    playos_ds1302_read(pddev, RTC_YEAR_ADDR, &year);

    dev_dbg(dev, "raw data is sec=%02x, min=%02x, hr=%02x, "
        "wday=%02x, mday=%02x, mon=%02x, year=%02x\n",
        seconds, minutes, hours, day, date, month, year);

    tm->tm_sec = ((seconds & 0x70) >> 4) * 10 + (seconds & 0x0F);
    tm->tm_min = ((minutes & 0x70) >> 4) * 10 + (minutes & 0x0F);
    if (hours & 0x80) { // 12 hour
        tm->tm_hour = 12 * ((hours & 0x20) >> 4) + (hours & 0x1F) - 1;
    } else {
        tm->tm_hour = ((hours & 0x20) >> 4) * 10 + (hours & 0x1F);
    }
    tm->tm_wday = (day & 0x7) -1 ;
    tm->tm_mday = ((date & 0x30) >> 4) * 10 + (date & 0x0F) - 1;
    tm->tm_mon = ((month & 0x10) >> 4) * 10 + (month & 0x0F) - 1;
    tm->tm_year = 2000 - 1900 + ((year & 0xF0) >> 4) * 10 + (year & 0x0F);
    
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
    int year = 1900 - 2000 + tm->tm_year;
    int mday = tm->tm_mday + 1;
    int mon = tm->tm_mon + 1;

    pr_err(">>>: %d\n", tm->tm_year);
    playos_ds1302_write(pddev, RTC_SECOND_ADDR, (tm->tm_sec % 10) | ((tm->tm_sec / 10) << 4));
    playos_ds1302_write(pddev, RTC_MINUTE_ADDR, (tm->tm_min % 10) | ((tm->tm_min / 10) << 4));
    if (tm->tm_hour >= 20) {
        playos_ds1302_write(pddev, RTC_HOUR_ADDR, (tm->tm_hour - 10) | 0x20);
    } else {
        playos_ds1302_write(pddev, RTC_HOUR_ADDR, tm->tm_hour);
    }
    playos_ds1302_write(pddev, RTC_DAY_ADDR, tm->tm_wday + 1);
    playos_ds1302_write(pddev, RTC_DATE_ADDR, (mday % 10) | ((mday / 10) << 4));
    playos_ds1302_write(pddev, RTC_MONTH_ADDR, (mon % 10) | ((mon / 10) << 4));
    playos_ds1302_write(pddev, RTC_YEAR_ADDR, (year % 10) | ((year / 10) << 4));

    return 0;
}

// static int playos_ds1302_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
// {
//     return -ENOSYS;
// }

// static int playos_ds1302_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
// {
//     return -ENOSYS;
// }

// static int playos_ds1302_proc(struct device *dev, struct seq_file *sfile)
// {
//     return -ENOSYS;
// }

// static int playos_ds1302_alarm_irq_enable(struct device *dev, unsigned int enabled)
// {
//     return -ENOSYS;
// }

// static int playos_ds1302_read_offset(struct device *dev, long *offset)
// {
//     return -ENOSYS;
// }

// static int playos_ds1302_set_offset(struct device *dev, long offset)
// {
//     return -ENOSYS;
// }


static const struct rtc_class_ops playos_ds1302_ops = {
    // .ioctl = playos_ds1302_ioctl,
    .read_time = playos_ds1302_read_time,
    .set_time = playos_ds1302_set_time,
    // .read_alarm = playos_ds1302_read_alarm,
    // .set_alarm = playos_ds1302_set_alarm,
    // .proc = playos_ds1302_proc,
    // .alarm_irq_enable = playos_ds1302_alarm_irq_enable,
    // .read_offset = playos_ds1302_read_offset,
    // .set_offset = playos_ds1302_set_offset,
};

static int playos_ds1302_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct playos_ds1302_device *ds1302 = 
            devm_kzalloc(&pdev->dev, sizeof(struct playos_ds1302_device), GFP_KERNEL);
    if (ds1302 == NULL) {
        return -ENOMEM;
    }

    ds1302->pdev = pdev;
    platform_set_drvdata(pdev, ds1302);

    ds1302->ce = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW);
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
