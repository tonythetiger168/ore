// mining_kmod.c -- Linux platform driver skeleton (kernel tree module).
// Replaces /dev/mem poking with proper device nodes + IRQ. Build inside a
// RISC-V kernel tree (make M=...); DT binding matches mining-accel.dtsi.
// SKELETON: compiles against kernel headers only (not host gcc).
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/poll.h>

struct mining_priv { void __iomem *base; int irq; wait_queue_head_t wq; atomic_t found; };

static irqreturn_t mining_irq(int irq, void *dev) {
    struct mining_priv *p = dev;
    if (readl(p->base + 0xA4) & 1) { atomic_set(&p->found, 1); wake_up(&p->wq); }
    return IRQ_HANDLED;
}
static ssize_t mining_read(struct file *f, char __user *b, size_t n, loff_t *o) {
    struct mining_priv *p = container_of(f->private_data, struct mining_priv, mdev);
    u32 off = *o;
    if (off > 0x100 || n != 4) return -EINVAL;
    if (put_user(readl(p->base + off), (u32 __user *)b)) return -EFAULT;
    return 4;
}
static ssize_t mining_write(struct file *f, const char __user *b, size_t n, loff_t *o) {
    struct mining_priv *p = container_of(f->private_data, struct mining_priv, mdev);
    u32 off = *o, v;
    if (off > 0x100 || n != 4) return -EINVAL;
    if (get_user(v, (u32 __user *)b)) return -EFAULT;
    writel(v, p->base + off);
    return 4;
}
static unsigned mining_poll(struct file *f, poll_table *t) {
    struct mining_priv *p = container_of(f->private_data, struct mining_priv, mdev);
    poll_wait(f, &p->wq, t);
    return atomic_read(&p->found) ? POLLIN : 0;
}
static const struct file_operations mining_fops = {
    .owner = THIS_MODULE, .read = mining_read, .write = mining_write, .poll = mining_poll,
};
static int mining_probe(struct platform_device *pdev) {
    struct mining_priv *p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
    struct resource *r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    p->base = devm_ioremap_resource(&pdev->dev, r);
    init_waitqueue_head(&p->wq);
    p->mdev.minor = MISC_DYNAMIC_MINOR; p->mdev.name = "mining0"; p->mdev.fops = &mining_fops;
    p->irq = platform_get_irq(pdev, 0);
    devm_request_irq(&pdev->dev, p->irq, mining_irq, 0, "mining", p);
    platform_set_drvdata(pdev, p);
    return misc_register(&p->mdev);
}
static int mining_remove(struct platform_device *pdev) {
    struct mining_priv *p = platform_get_drvdata(pdev);
    misc_deregister(&p->mdev);
    return 0;
}
static const struct of_device_id mining_ids[] = { { .compatible = "edu,mining-accel" }, {} };
MODULE_DEVICE_TABLE(of, mining_ids);
static struct platform_driver mining_drv = {
    .probe = mining_probe, .remove = mining_remove,
    .driver = { .name = "mining-accel", .of_match_table = mining_ids },
};
module_platform_driver(mining_drv);
MODULE_LICENSE("Apache-2.0");
