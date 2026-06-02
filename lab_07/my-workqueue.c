#include "layout.h"
#include <linux/input-event-codes.h>

#include <asm/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Divaev");

#define IRQ_NO 1
#define IRQ_NAME "irq-workqueue"
#define WQ_NAME "my-workqueue"

struct my_work_struct
{
    struct work_struct work;
    int code;
};

typedef struct my_work_struct my_work_struct_t;

static struct workqueue_struct *workqueue;
static my_work_struct_t *work1, *work2;

static size_t layout_size = 0;

static void work_function_1(struct work_struct *work)
{
    my_work_struct_t *my_work = (my_work_struct_t *)work;
    int code = my_work->code;

    if (!(code & 0x80))
        return;

    code &= 0x7F;

    if (code < layout_size)
        printk(KERN_DEBUG "workqueue: %s (%d)\n", layout[code], code);
    else
        printk(KERN_DEBUG "workqueue: UNKNOWN (%d)\n", code);
}

static void work_function_2(struct work_struct *work)
{
    msleep(2);
    my_work_struct_t *my_work = (my_work_struct_t *)work;
    int code = my_work->code;

    if (!(code & 0x80))
        return;

    code &= 0x7F;

    if (code < layout_size)
        printk(KERN_DEBUG "workqueue2: %s (%d)\n", layout[code], code);
    else
        printk(KERN_DEBUG "workqueue2: UNKNOWN (%d)\n", code);
}

static irqreturn_t irq_handler_workqueue(int irq_no, void *dev_id)
{
    if (IRQ_NO == irq_no)
    {
        int code = inb(0x60);

        work1->code = code;
        work2->code = code;

        queue_work(workqueue, (struct work_struct *)work1);
        queue_work(workqueue, (struct work_struct *)work2);

        return IRQ_HANDLED;
    }
    return IRQ_NONE;
}

static int __init my_workqueue_init(void)
{
    printk(KERN_DEBUG "workqueue: init\n");

    int ret = request_irq(IRQ_NO, irq_handler_workqueue, IRQF_SHARED, IRQ_NAME,
                          (void *)(irq_handler_workqueue));
    if (ret != 0)
    {
        printk(KERN_ERR "workqueue: request_irq\n");
        return ret;
    }

    workqueue = create_workqueue(WQ_NAME);
    if (workqueue == NULL)
    {
        free_irq(IRQ_NO, (void *)(irq_handler_workqueue));
        printk(KERN_ERR "workqueue: create_workqueue\n");
        return -ENOMEM;
    }

    work1 = kmalloc(sizeof(my_work_struct_t), GFP_KERNEL);
    if (work1 == NULL)
    {
        free_irq(IRQ_NO, (void *)(irq_handler_workqueue));
        destroy_workqueue(workqueue);
        printk(KERN_ERR "workqueue: kmalloc\n");
        return -ENOMEM;
    }
    work2 = kmalloc(sizeof(my_work_struct_t), GFP_KERNEL);
    if (work2 == NULL)
    {
        free_irq(IRQ_NO, (void *)(irq_handler_workqueue));
        destroy_workqueue(workqueue);
        kfree(work1);
        printk(KERN_ERR "workqueue: kmalloc\n");
        return -ENOMEM;
    }

    INIT_WORK((struct work_struct *)work1, work_function_1);
    INIT_WORK((struct work_struct *)work2, work_function_2);

    layout_size = ARRAY_SIZE(layout);

    printk(KERN_DEBUG "workqueue: worqueue setted\n");

    return 0;
}

static void __exit my_workqueue_exit(void)
{
    flush_workqueue(workqueue);
    printk(KERN_DEBUG "workqueue: exit\n");
    destroy_workqueue(workqueue);
    kfree(work1);
    kfree(work2);
    free_irq(IRQ_NO, (void *)(irq_handler_workqueue));
}

module_init(my_workqueue_init);
module_exit(my_workqueue_exit);
