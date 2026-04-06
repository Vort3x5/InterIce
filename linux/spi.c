#include <linux/module.h>
#include <linux/usb.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define FTDI_VID 0x0403
#define FTDI_PID 0x6010

#define EP_TX 4
#define EP_RX 3

static struct usb_device *udev = NULL;
static struct kobject *sykt_kobj;

static ssize_t x_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u32 val;
    u8 *tx;
    int actual, ret;
    if (!udev) return -ENODEV;
    if (kstrtou32(buf, 0, &val) < 0) return -EINVAL;

    tx = kmalloc(32, GFP_KERNEL);
    if (!tx) return -ENOMEM;

    tx[0]  = 0x80; tx[1]  = 0x00; tx[2]  = 0x0B;
    tx[3]  = 0x11; tx[4]  = 0x03; tx[5]  = 0x00;
    tx[6]  = 0x02;
    tx[7]  = (val >> 16) & 0xFF;
    tx[8]  = (val >>  8) & 0xFF;
    tx[9]  =  val        & 0xFF;
    tx[10] = 0x87;
    tx[11] = 0x80; tx[12] = 0x08; tx[13] = 0x0B;
    tx[14] = 0x87;

    ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, EP_TX), tx, 15, &actual, 1000);
    if (ret)
        pr_err("quadra: x_store bulk write failed: %d\n", ret);

    kfree(tx);
    return count;
}

static ssize_t ctrl_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u8 *tx;
    int actual, ret;
    if (!udev) return -ENODEV;

    tx = kmalloc(32, GFP_KERNEL);
    if (!tx) return -ENOMEM;

    tx[0]  = 0x80; tx[1]  = 0x00; tx[2]  = 0x0B;
    tx[3]  = 0x11; tx[4]  = 0x01; tx[5]  = 0x00;
    tx[6]  = 0x04; tx[7]  = 0x01;
    tx[8]  = 0x87;
    tx[9]  = 0x80; tx[10] = 0x08; tx[11] = 0x0B;
    tx[12] = 0x87;

    ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, EP_TX), tx, 13, &actual, 1000);
    if (ret)
        pr_err("quadra: ctrl_store bulk write failed: %d\n", ret);

    kfree(tx);
    return count;
}

static ssize_t y_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u8 *tx, *rx;
    u32 val = 0;
    int actual, ret, i;
    int payload_len = 0;
    int tries = 0;
    u8 payload[4] = {0};

    if (!udev) return -ENODEV;

    tx = kmalloc(512, GFP_KERNEL);
    rx = kmalloc(512, GFP_KERNEL);
    if (!tx || !rx) {
        kfree(tx); kfree(rx);
        return -ENOMEM;
    }

    for (i = 0; i < 10; i++) {
        if (usb_bulk_msg(udev, usb_rcvbulkpipe(udev, EP_RX), rx, 512, &actual, 5) != 0 || actual == 0)
            break;
    }

    tx[0]  = 0x80; tx[1]  = 0x00; tx[2]  = 0x0B;
    tx[3]  = 0x11; tx[4]  = 0x00; tx[5]  = 0x00; tx[6]  = 0x09;
    tx[7]  = 0x20; tx[8]  = 0x03; tx[9]  = 0x00; tx[10] = 0x87;
    tx[11] = 0x80; tx[12] = 0x08; tx[13] = 0x0B;
    tx[14] = 0x87;

    ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, EP_TX), tx, 15, &actual, 1000);
    if (ret) {
        pr_err("quadra: y_show bulk write failed: %d\n", ret);
        kfree(tx); kfree(rx);
        return -EIO;
    }

    while (payload_len < 4 && tries++ < 20) {
        ret = usb_bulk_msg(udev, usb_rcvbulkpipe(udev, EP_RX), rx, 512, &actual, 100);
        if (ret || actual < 2) continue;

        pr_info("quadra: rx %d bytes: %02x %02x %02x %02x %02x %02x\n",
                actual,
                actual > 0 ? rx[0] : 0, actual > 1 ? rx[1] : 0,
                actual > 2 ? rx[2] : 0, actual > 3 ? rx[3] : 0,
                actual > 4 ? rx[4] : 0, actual > 5 ? rx[5] : 0);

        for (i = 2; i < actual && payload_len < 4; i++)
            payload[payload_len++] = rx[i];
    }

    pr_info("quadra: payload=%02x %02x %02x %02x (len=%d)\n",
            payload[0], payload[1], payload[2], payload[3], payload_len);

    if (payload_len == 4)
        val = (((u32)payload[0] << 24) | ((u32)payload[1] << 16) |
               ((u32)payload[2] <<  8) |  (u32)payload[3]) >> 7;

    kfree(tx);
    kfree(rx);

    if (payload_len < 4) return -EIO;
    return sprintf(buf, "%u\n", val);
}

static struct kobj_attribute x_attr    = __ATTR(dsskma, 0220, NULL, x_store);
static struct kobj_attribute ctrl_attr = __ATTR(dtskma, 0220, NULL, ctrl_store);
static struct kobj_attribute y_attr    = __ATTR(drskma, 0444, y_show, NULL);

static struct attribute *attrs[] = { &x_attr.attr, &ctrl_attr.attr, &y_attr.attr, NULL };
static struct attribute_group attr_group = { .attrs = attrs };

static int quadra_usb_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    u8 *buf;
    int actual, i;

    if (intf->cur_altsetting->desc.bInterfaceNumber != 1) return -ENODEV;
    udev = interface_to_usbdev(intf);

    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x00, 0x40, 0,      2, NULL, 0, 1000);
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x09, 0x40, 1,      2, NULL, 0, 1000);
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0B, 0x40, 0x020B, 2, NULL, 0, 1000);

    msleep(20);

    buf = kmalloc(32, GFP_KERNEL);
    if (!buf) return -ENOMEM;

    i = 0;
    buf[i++] = 0x8A;
    buf[i++] = 0x97;
    buf[i++] = 0x8D;
    buf[i++] = 0x86; buf[i++] = 0x1D; buf[i++] = 0x00;
    buf[i++] = 0x80; buf[i++] = 0x08; buf[i++] = 0x0B;
    buf[i++] = 0x87;

    usb_bulk_msg(udev, usb_sndbulkpipe(udev, EP_TX), buf, i, &actual, 1000);
    kfree(buf);

    msleep(10);

    buf = kmalloc(64, GFP_KERNEL);
    if (buf) {
        for (i = 0; i < 5; i++) {
            if (usb_bulk_msg(udev, usb_rcvbulkpipe(udev, EP_RX), buf, 64, &actual, 10) != 0 || actual == 0)
                break;
            if (actual > 2 && buf[2] == 0xFA)
                pr_err("quadra: MPSSE bad command echo: 0xFA 0x%02x\n", buf[3]);
        }
        kfree(buf);
    }

    sykt_kobj = kobject_create_and_add("sykt_sysfs", kernel_kobj);
    if (!sykt_kobj) return -ENOMEM;

    if (sysfs_create_group(sykt_kobj, &attr_group)) {
        kobject_put(sykt_kobj);
        return -ENOMEM;
    }

    pr_info("quadra: probed OK (EP_TX=%d OUT, EP_RX=%d IN)\n", EP_TX, EP_RX);
    return 0;
}

static void quadra_usb_disconnect(struct usb_interface *intf) {
    if (intf->cur_altsetting->desc.bInterfaceNumber == 1) {
        if (sykt_kobj) {
            sysfs_remove_group(sykt_kobj, &attr_group);
            kobject_put(sykt_kobj);
            sykt_kobj = NULL;
        }
        udev = NULL;
    }
}

static const struct usb_device_id ftdi_id_table[] = {
    { USB_DEVICE(FTDI_VID, FTDI_PID) },
    { }
};
MODULE_DEVICE_TABLE(usb, ftdi_id_table);

static struct usb_driver quadra_usb_driver = {
    .name       = "quadra_usb",
    .id_table   = ftdi_id_table,
    .probe      = quadra_usb_probe,
    .disconnect = quadra_usb_disconnect,
};

module_usb_driver(quadra_usb_driver);
MODULE_LICENSE("GPL");
