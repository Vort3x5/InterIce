#include <linux/module.h>
#include <linux/usb.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/slab.h>

#define FTDI_VID 0x0403
#define FTDI_PID 0x6010

static struct usb_device *udev = NULL;
static struct kobject *sykt_kobj;

static int send_mpsse(u8 *cmd, int len) {
    int actual;
    return usb_bulk_msg(udev, usb_sndbulkpipe(udev, 4), cmd, len, &actual, 1000);
}

static int read_mpsse(u8 *cmd, int cmd_len, u8 *rx, int rx_len) {
    int actual;
    int ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, 4), cmd, cmd_len, &actual, 1000);
    if (ret) return ret;
    return usb_bulk_msg(udev, usb_rcvbulkpipe(udev, 3), rx, rx_len, &actual, 1000);
}

static ssize_t x_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u32 val;
    u8 *tx;
    if (!udev) return -ENODEV;
    if (kstrtou32(buf, 10, &val) < 0) return -EINVAL;

    tx = kmalloc(16, GFP_KERNEL);
    if (!tx) return -ENOMEM;

    tx[0] = 0x80; tx[1] = 0x00; tx[2] = 0x0B;
    send_mpsse(tx, 3);

    tx[0] = 0x11; tx[1] = 0x03; tx[2] = 0x00;
    tx[3] = 0x02; tx[4] = (val >> 16) & 0xFF; tx[5] = (val >> 8) & 0xFF; tx[6] = val & 0xFF;
    send_mpsse(tx, 7);

    tx[0] = 0x80; tx[1] = 0x08; tx[2] = 0x0B;
    send_mpsse(tx, 3);

    kfree(tx);
    return count;
}

static ssize_t ctrl_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u8 *tx;
    if (!udev) return -ENODEV;

    tx = kmalloc(16, GFP_KERNEL);
    if (!tx) return -ENOMEM;

    tx[0] = 0x80; tx[1] = 0x00; tx[2] = 0x0B; send_mpsse(tx, 3);
    tx[0] = 0x11; tx[1] = 0x01; tx[2] = 0x00; tx[3] = 0x04; tx[4] = 0x01; send_mpsse(tx, 5);
    tx[0] = 0x80; tx[1] = 0x08; tx[2] = 0x0B; send_mpsse(tx, 3);

    kfree(tx);
    return count;
}

static ssize_t y_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u8 *tx, *rx;
    u32 val;
    if (!udev) return -ENODEV;

    tx = kmalloc(16, GFP_KERNEL);
    rx = kmalloc(16, GFP_KERNEL);
    if (!tx || !rx) {
        kfree(tx); kfree(rx);
        return -ENOMEM;
    }

    tx[0] = 0x80; tx[1] = 0x00; tx[2] = 0x0B; 
    send_mpsse(tx, 3);

    tx[0] = 0x11; tx[1] = 0x00; tx[2] = 0x00; tx[3] = 0x09; 
    send_mpsse(tx, 4);

    tx[0] = 0x20; tx[1] = 0x03; tx[2] = 0x00; tx[3] = 0x87;
    read_mpsse(tx, 4, rx, 6);

    tx[0] = 0x80; tx[1] = 0x08; tx[2] = 0x0B; 
    send_mpsse(tx, 3);

    val = (rx[2] << 24) | (rx[3] << 16) | (rx[4] << 8) | rx[5];
    
    kfree(tx); 
    kfree(rx);
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

    // Wymuszenie indeksu 2 - przekierowanie instrukcji sprzętowych do Kanału B
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0, 0x40, 0, 2, NULL, 0, 1000);
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 9, 0x40, 1, 2, NULL, 0, 1000);
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0B, 0x40, 0x020B, 2, NULL, 0, 1000);

    init_tx = kmalloc(16, GFP_KERNEL);
    if (init_tx) {
        int i = 0;
        init_tx[i++] = 0x8A; // Wyłączenie dzielnika bazowego /5
        init_tx[i++] = 0x97; // Wyłączenie adaptacyjnego taktowania
        init_tx[i++] = 0x8D; // Wyłączenie zegara 3-fazowego
        init_tx[i++] = 0x86; init_tx[i++] = 0x02; init_tx[i++] = 0x00; // Inicjalizacja TCK na 10 MHz
        init_tx[i++] = 0x80; init_tx[i++] = 0x08; init_tx[i++] = 0x0B; // Ustawienie CS w stan wysoki
        send_mpsse(init_tx, i);
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
