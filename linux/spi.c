#include <linux/module.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define FTDI_VID 0x0403
#define FTDI_PID 0x6010
#define EP_TX 4
#define EP_RX 3

struct quadra_batch {
    u32 count;
    u32 __user *x_in;
    u32 __user *y_out;
};
#define QUADRA_IOC_BATCH _IOWR('q', 1, struct quadra_batch)

static struct usb_device *udev = NULL;
static int major;
static struct class *quadra_class;
static DEFINE_MUTEX(quadra_mutex);

static long quadra_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct quadra_batch batch;
    u32 *x_buf, *y_buf;
    u8 *tx, *rx, *payload;
    int ret = 0, actual, i, total_processed = 0;

    if (cmd != QUADRA_IOC_BATCH) return -ENOTTY;
    if (copy_from_user(&batch, (void __user *)arg, sizeof(batch))) return -EFAULT;
    if (batch.count == 0 || batch.count > 1000000) return -EINVAL;

    x_buf = kvmalloc_array(batch.count, sizeof(u32), GFP_KERNEL);
    y_buf = kvmalloc_array(batch.count, sizeof(u32), GFP_KERNEL);
    tx = kmalloc(4096, GFP_KERNEL); 
    rx = kmalloc(512, GFP_KERNEL);
    payload = kmalloc(512, GFP_KERNEL);

    if (!x_buf || !y_buf || !tx || !rx || !payload) {
        ret = -ENOMEM;
        goto out;
    }

    if (copy_from_user(x_buf, batch.x_in, batch.count * sizeof(u32))) {
        ret = -EFAULT;
        goto out;
    }

    mutex_lock(&quadra_mutex);

    for (i = 0; i < 10; i++) {
        if (usb_bulk_msg(udev, usb_rcvbulkpipe(udev, EP_RX), rx, 512, &actual, 5) != 0 || actual == 0) break;
    }

    while (total_processed < batch.count) {
        int remaining = batch.count - total_processed;
        int chunk_sz = (remaining > 64) ? 64 : remaining;
        int tx_len = 0;
        int payload_needed = chunk_sz * 4;
        int payload_got = 0;
        int tries = 0;

        for (i = 0; i < chunk_sz; i++) {
            u32 val = x_buf[total_processed + i];
            
            tx[tx_len++] = 0x80; tx[tx_len++] = 0x00; tx[tx_len++] = 0x0B;
            tx[tx_len++] = 0x11; tx[tx_len++] = 0x03; tx[tx_len++] = 0x00;
            tx[tx_len++] = 0x02; tx[tx_len++] = (val >> 16) & 0xFF; tx[tx_len++] = (val >> 8) & 0xFF; tx[tx_len++] = val & 0xFF;
            tx[tx_len++] = 0x80; tx[tx_len++] = 0x08; tx[tx_len++] = 0x0B;
            
            tx[tx_len++] = 0x80; tx[tx_len++] = 0x00; tx[tx_len++] = 0x0B;
            tx[tx_len++] = 0x11; tx[tx_len++] = 0x01; tx[tx_len++] = 0x00;
            tx[tx_len++] = 0x04; tx[tx_len++] = 0x01;
            tx[tx_len++] = 0x80; tx[tx_len++] = 0x08; tx[tx_len++] = 0x0B;
            
            tx[tx_len++] = 0x80; tx[tx_len++] = 0x00; tx[tx_len++] = 0x0B;
            tx[tx_len++] = 0x11; tx[tx_len++] = 0x00; tx[tx_len++] = 0x00; tx[tx_len++] = 0x09;
            tx[tx_len++] = 0x20; tx[tx_len++] = 0x03; tx[tx_len++] = 0x00; 
            tx[tx_len++] = 0x80; tx[tx_len++] = 0x08; tx[tx_len++] = 0x0B; 
        }
        tx[tx_len++] = 0x87; 

        ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, EP_TX), tx, tx_len, &actual, 1000);
        if (ret) break;

        while (payload_got < payload_needed && tries++ < 50) {
            ret = usb_bulk_msg(udev, usb_rcvbulkpipe(udev, EP_RX), rx, 512, &actual, 100);
            if (ret || actual < 2) continue;
            
            for (i = 2; i < actual && payload_got < payload_needed; i++) {
                payload[payload_got++] = rx[i];
            }
        }

        if (payload_got < payload_needed) {
            ret = -EIO;
            break;
        }

        for (i = 0; i < chunk_sz; i++) {
            u32 r = ((u32)payload[i*4] << 24) | ((u32)payload[i*4+1] << 16) | ((u32)payload[i*4+2] << 8) | (u32)payload[i*4+3];
            y_buf[total_processed + i] = r >> 7;
        }
        total_processed += chunk_sz;
    }

    mutex_unlock(&quadra_mutex);

    if (!ret && copy_to_user(batch.y_out, y_buf, batch.count * sizeof(u32))) {
        ret = -EFAULT;
    }

out:
    kvfree(x_buf);
    kvfree(y_buf);
    kfree(tx);
    kfree(rx);
    kfree(payload);
    return ret;
}

static const struct file_operations quadra_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = quadra_ioctl,
};

static int quadra_usb_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    u8 *init_tx;
    int i, actual;

    if (intf->cur_altsetting->desc.bInterfaceNumber != 1) return -ENODEV;
    udev = interface_to_usbdev(intf);

    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0, 0x40, 0, 2, NULL, 0, 1000);
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 9, 0x40, 1, 2, NULL, 0, 1000);
    usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0B, 0x40, 0x020B, 2, NULL, 0, 1000);

    init_tx = kmalloc(32, GFP_KERNEL);
    if (init_tx) {
        i = 0;
        init_tx[i++] = 0x8A; 
        init_tx[i++] = 0x97; 
        init_tx[i++] = 0x8D; 
        init_tx[i++] = 0x86; init_tx[i++] = 0x1D; init_tx[i++] = 0x00; 
        init_tx[i++] = 0x80; init_tx[i++] = 0x08; init_tx[i++] = 0x0B; 
        usb_bulk_msg(udev, usb_sndbulkpipe(udev, EP_TX), init_tx, i, &actual, 1000);
        kfree(init_tx);
    }

    major = register_chrdev(0, "quadra", &quadra_fops);
    if (major < 0) return major;

    quadra_class = class_create("quadra");
    if (IS_ERR(quadra_class)) {
        unregister_chrdev(major, "quadra");
        return PTR_ERR(quadra_class);
    }

    device_create(quadra_class, NULL, MKDEV(major, 0), NULL, "quadra");
    return 0;
}

static void quadra_usb_disconnect(struct usb_interface *intf) {
    if (intf->cur_altsetting->desc.bInterfaceNumber == 1) {
        device_destroy(quadra_class, MKDEV(major, 0));
        class_destroy(quadra_class);
        unregister_chrdev(major, "quadra");
        udev = NULL;
    }
}

static const struct usb_device_id ftdi_id_table[] = { { USB_DEVICE(FTDI_VID, FTDI_PID) }, { } };
MODULE_DEVICE_TABLE(usb, ftdi_id_table);

static struct usb_driver quadra_usb_driver = {
    .name       = "quadra_usb",
    .id_table   = ftdi_id_table,
    .probe      = quadra_usb_probe,
    .disconnect = quadra_usb_disconnect,
};

module_usb_driver(quadra_usb_driver);
MODULE_LICENSE("GPL");
