#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

static struct kobject *sykt_kobj;
static struct spi_device *quadra_spi_dev = NULL;

static ssize_t x_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u32 val;
    u8 tx[4];
    if (!quadra_spi_dev) return -ENODEV;
    if (kstrtou32(buf, 10, &val) < 0) return -EINVAL;
    
    tx[0] = 0x02; 
    tx[1] = (val >> 16) & 0xFF;
    tx[2] = (val >> 8) & 0xFF;
    tx[3] = val & 0xFF;
    
    spi_write(quadra_spi_dev, tx, 4);
    return count;
}

static ssize_t ctrl_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u8 tx[2] = {0x04, 0x01};
    if (!quadra_spi_dev) return -ENODEV;
    spi_write(quadra_spi_dev, tx, 2);
    return count;
}

static ssize_t y_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u8 tx[5] = {0x09, 0, 0, 0, 0}; 
    u8 rx[5] = {0};
    struct spi_transfer t = { .tx_buf = tx, .rx_buf = rx, .len = 5 };
    struct spi_message m;
    u32 val;

    if (!quadra_spi_dev) return -ENODEV;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    spi_sync(quadra_spi_dev, &m);

    val = (rx[1] << 24) | (rx[2] << 16) | (rx[3] << 8) | rx[4];
    return sprintf(buf, "%u\n", val);
}

static struct kobj_attribute x_attr = __ATTR(dsskma, 0220, NULL, x_store);
static struct kobj_attribute ctrl_attr = __ATTR(dtskma, 0220, NULL, ctrl_store);
static struct kobj_attribute y_attr = __ATTR(drskma, 0444, y_show, NULL);

static struct attribute *attrs[] = {
    &x_attr.attr,
    &ctrl_attr.attr,
    &y_attr.attr,
    NULL,
};
static struct attribute_group attr_group = { .attrs = attrs };

static int quadra_spi_probe(struct spi_device *spi) {
    quadra_spi_dev = spi;
    spi->mode = SPI_MODE_0;
    spi->bits_per_word = 8;
    spi_setup(spi);

    sykt_kobj = kobject_create_and_add("sykt_sysfs", kernel_kobj);
    if (!sykt_kobj) return -ENOMEM;
    
    if (sysfs_create_group(sykt_kobj, &attr_group)) {
        kobject_put(sykt_kobj);
        return -ENOMEM;
    }
    return 0;
}

static void quadra_spi_remove(struct spi_device *spi) {
    if (sykt_kobj) {
        sysfs_remove_group(sykt_kobj, &attr_group);
        kobject_put(sykt_kobj);
        sykt_kobj = NULL;
    }
    quadra_spi_dev = NULL;
}

static const struct spi_device_id quadra_id[] = {
    { "quadra_spi", 0 },
    { "spidev", 0 }, 
    { }
};
MODULE_DEVICE_TABLE(spi, quadra_id);

static struct spi_driver quadra_spi_driver = {
    .driver = {
        .name = "quadra_spi",
        .owner = THIS_MODULE,
    },
    .probe = quadra_spi_probe,
    .remove = quadra_spi_remove,
    .id_table = quadra_id,
};

module_spi_driver(quadra_spi_driver);
MODULE_LICENSE("GPL");
