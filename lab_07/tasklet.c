#include "layout.h"
#include <linux/input-event-codes.h>

#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Divaev");

#define IRQ_NO 1
#define IRQ_NAME "my-tasklet"

static struct tasklet_struct *tasklet = NULL;

static size_t layout_size = 0;

static void tasklet_function(unsigned long code)
{
    if (!(code & 0x80))
        return;
    code &= 0x7F;

    if (code < layout_size)
        printk(KERN_DEBUG "tasklet: %s (%lu)\n", layout[code], code);
    else
        printk(KERN_DEBUG "tasklet: UNKNOWN (%lu)\n", code);
}

static irqreturn_t irq_handler_tasklet(int irq_no, void *dev_id)
{
    if (IRQ_NO == irq_no)
    {
        tasklet->data = inb(0x60);

        tasklet_schedule(tasklet);
        return IRQ_HANDLED;
    }
    return IRQ_NONE;
}

static int __init my_tasklet_init(void)
{
    printk(KERN_DEBUG "ttasklet: init\n");

    int ret = request_irq(IRQ_NO, irq_handler_tasklet, IRQF_SHARED, IRQ_NAME,
                          (void *)(irq_handler_tasklet));
    if (ret != 0)
    {
        printk(KERN_ERR "tasklet: request_irq\n");
        return ret;
    }

    tasklet = kmalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
    if (tasklet == NULL)
    {
        free_irq(IRQ_NO, (void *)(irq_handler_tasklet));
        printk(KERN_ERR "tasklet: kmalloc\n");
        return -ENOMEM;
    }

    tasklet_init(tasklet, tasklet_function, 0);
    layout_size = ARRAY_SIZE(layout);

    printk(KERN_DEBUG "tasklet: tasklet setted\n");

    return 0;
}

static void __exit my_tasklet_exit(void)
{
    printk(KERN_DEBUG "tasklet: exit\n");
    tasklet_kill(tasklet);
    kfree(tasklet);
    free_irq(IRQ_NO, (void *)(irq_handler_tasklet));
}

module_init(my_tasklet_init);
module_exit(my_tasklet_exit);
