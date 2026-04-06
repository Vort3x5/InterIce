#include <linux/module.h>
#include <linux/usb.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/slab.h>

#define FTDI_VID 0x0403
#define FTDI_PID 0x6010

static struct usb_device *udev = NULL;
static struct kobject *sykt_kobj;

static ssize_t x_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u32 val;
    u8 *tx;
    int actual;
    if (!udev) return -ENODEV;
    if (kstrtou32(buf, 10, &val) < 0) return -EINVAL;

    tx = kmalloc(32, GFP_KERNEL);
    if (!tx) return -ENOMEM;

    tx[0] = 0x80; tx[1] = 0x00; tx[2] = 0x0B;
    tx[3] = 0x11; tx[4] = 0x03; tx[5] = 0x00;
    tx[6] = 0x02; tx[7] = (val >> 16) & 0xFF; tx[8] = (val >> 8) & 0xFF; tx[9] = val & 0xFF;
    tx[10] = 0x80; tx[11] = 0x08; tx[12] = 0x0B;
    
    usb_bulk_msg(udev, usb_sndbulkpipe(udev, 4), tx, 13, &actual, 1000);
    kfree(tx);
    return count;
}

static ssize_t ctrl_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u8 *tx;
    int actual;
    if (!udev) return -ENODEV;

    tx = kmalloc(32, GFP_KERNEL);
    if (!tx) return -ENOMEM;

    tx[0] = 0x80; tx[1] = 0x00; tx[2] = 0x0B;
    tx[3] = 0x11; tx[4] = 0x01; tx[5] = 0x00;
    tx[6] = 0x04; tx[7] = 0x01;
    tx[8] = 0x80; tx[9] = 0x08; tx[10] = 0x0B;
    
    usb_bulk_msg(udev, usb_sndbulkpipe(udev, 4), tx, 11, &actual, 1000);
    kfree(tx);
    return count;
}

static ssize_t y_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u8 *tx, *rx;
    u32 val = 0;
    int actual, ret, i;
    int payload_len = 0;
    u8 payload[4] = {0};

    if (!udev) return -ENODEV;

    tx = kmalloc(512, GFP_KERNEL);
    rx = kmalloc(512, GFP_KERNEL);
    if (!tx || !rx) {
        kfree(tx); kfree(rx);
        return -ENOMEM;
    }

    for (i = 0; i < 10; i++) {
        if (usb_bulk_msg(udev, usb_rcvbulkpipe(udev, 3), rx, 512, &actual, 5) != 0 || actual == 0) 
            break;
    }

    tx[0] = 0x80; tx[1] = 0x00; tx[2] = 0x0B;
    tx[3] = 0x11; tx[4] = 0x00; tx[5] = 0x00; tx[6] = 0x09;
    tx[7] = 0x20; tx[8] = 0x03; tx[9] = 0x00; tx[10] = 0x87;
    tx[11] = 0x80; tx[12] = 0x08; tx[13] = 0x0B;
    
    usb_bulk_msg(udev, usb_sndbulkpipe(udev, 4), tx, 14, &actual, 1000);

    while (payload_len < 4) {
        ret = usb_bulk_msg(udev, usb_rcvbulkpipe(udev, 3), rx, 512, &actual, 1000);
        if (ret || actual < 2) break;
        
        for (i = 2; i < actual && payload_len < 4; i++) {
            payload[payload_len++] = rx[i];
        }
    }

    if (payload_len == 4) {
        val = ((payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3]) >> 7;
    }

    kfree(tx); 
    kfree(rx);

    if (payload_len < 4) return -EIO;
    return sprintf(buf, "%u\n", val);
}

static struct kobj_attribute x_attr = __ATTR(dsskma, 0220, NULL, x_store);
static struct kobj_attribute ctrl_attr = __ATTR(dtskma, 0220, NULL, ctrl_store);
static struct kobj_attribute y_attr = __ATTR(drskma, 0444, y_show, NULL);

static struct attribute *attrs[] = { &x_attr.attr, &ctrl_attr.attr, &y_attr.attr, NULL };
static struct attribute_group attr_group = { .attrs = attrs };

static int quadra_usb_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    u8 *init_tx;
    
    if (intf->cur_altsetting->desc.bInterfaceNumber != 1) return -ENODEV;
    udev = interface_to_usbdev(intf);

    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0, 0x40, 0, 2, NULL, 0, 1000);
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 9, 0x40, 1, 2, NULL, 0, 1000);
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0B, 0x40, 0x020B, 2, NULL, 0, 1000);

    init_tx = kmalloc(32, GFP_KERNEL);
    if (init_tx) {
        int i = 0;
        init_tx[i++] = 0x8A; 
        init_tx[i++] = 0x97; 
        init_tx[i++] = 0x8D; 
        init_tx[i++] = 0x86; init_tx[i++] = 0x1D; init_tx[i++] = 0x00; 
        init_tx[i++] = 0x80; init_tx[i++] = 0x08; init_tx[i++] = 0x0B; 
        usb_bulk_msg(udev, usb_sndbulkpipe(udev, 4), init_tx, i, NULL, 1000);
        kfree(init_tx);
    }

    sykt_kobj = kobject_create_and_add("sykt_sysfs", kernel_kobj);
    if (!sykt_kobj) return -ENOMEM;
    
    if (sysfs_create_group(sykt_kobj, &attr_group)) {
        kobject_put(sykt_kobj);
        return -ENOMEM;
    }
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
