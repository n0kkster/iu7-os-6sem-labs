#include <linux/fs_context.h>

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Divaev");

#define VFS_MAGIC_NUMBER 0x67676767
#define SLAB_NAME "vfs_slab"

#define STRUCT_SIZE 32
#define ELEM_NUM 128

void **struct_arr = NULL;
static struct kmem_cache *vfs_slab;

static struct super_operations const vfs_super_operations = {
    .statfs = simple_statfs,
};

static int vfs_fill_sb(struct super_block *sb, struct fs_context *fc)
{
    struct inode *inode;

    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_magic = VFS_MAGIC_NUMBER;
    sb->s_op = &vfs_super_operations;

    inode = new_inode(sb);
    if (!inode)
    {
        printk(KERN_ERR "*** vfs: new_inode failed\n");
        return -ENOMEM;
    }
    printk(KERN_DEBUG "*** vfs: new inode created\n");
    inode->i_ino = 1;
    inode->i_mode = S_IFDIR | 0755;
    inode->i_size = PAGE_SIZE;

    simple_inode_init_ts(inode);
    inode->i_op = &simple_dir_inode_operations;
    inode->i_fop = &simple_dir_operations;

    struct timespec64 cur = current_time(inode);
    inode_set_atime_to_ts(inode, cur);
    inode_set_mtime_to_ts(inode, cur);
    inode_set_ctime_to_ts(inode, cur);

    sb->s_root = d_make_root(inode);
    if (!sb->s_root)
    {
        printk(KERN_ERR "*** vfs: d_make_root failed\n");
        iput(inode);
        return -ENOMEM;
    }

    printk(KERN_DEBUG "*** vfs: root created\n");
    return 0;
}

static int my_vfs_get_tree(struct fs_context *fc)
{
    printk(KERN_DEBUG "*** vfs: mount - get_tree\n");
    return get_tree_nodev(fc, vfs_fill_sb);
}

static const struct fs_context_operations vfs_context_ops = {
    .get_tree = my_vfs_get_tree,
};

static int vfs_init_fs_context(struct fs_context *fc)
{
    printk(KERN_DEBUG "*** vfs: init fs context\n");
    fc->ops = &vfs_context_ops;
    return 0;
}

static void vfs_free_memory(void)
{
    if (struct_arr)
    {
        for (int i = 0; i < ELEM_NUM; i++)
        {
            if (struct_arr[i])
                kmem_cache_free(vfs_slab, struct_arr[i]);
        }
        kfree(struct_arr);
    }
    if (vfs_slab)
    {
        kmem_cache_destroy(vfs_slab);
    }
    printk(KERN_DEBUG "*** vfs: slab destroyed\n");
}

static void kill_sb(struct super_block *sb)
{
    printk(KERN_DEBUG "*** vfs: kill superblock\n");
    kill_anon_super(sb);
}

struct file_system_type vfs_type = {
    .owner = THIS_MODULE,
    .name = "vfs",
    .init_fs_context = vfs_init_fs_context,
    .kill_sb = kill_sb,
    .fs_flags = FS_USERNS_MOUNT,
};

static int __init vfs_init(void)
{
    printk(KERN_DEBUG "*** vfs: init\n");

    vfs_slab = kmem_cache_create(SLAB_NAME, STRUCT_SIZE, 0,
                                 SLAB_HWCACHE_ALIGN | SLAB_NO_MERGE, NULL);
    if (!vfs_slab)
    {
        printk(KERN_ERR "*** vfs: kmem_cache_create failed\n");
        return -ENOMEM;
    }
    printk(KERN_DEBUG "*** vfs: slab cache created\n");

    struct_arr = kcalloc(ELEM_NUM, sizeof(void *), GFP_KERNEL);
    if (!struct_arr)
    {
        printk(KERN_ERR "*** vfs: kcalloc failed\n");
        vfs_free_memory();
        return -ENOMEM;
    }

    for (int i = 0; i < ELEM_NUM; i++)
    {
        struct_arr[i] = kmem_cache_alloc(vfs_slab, GFP_KERNEL);
        if (!struct_arr[i])
        {
            printk(KERN_ERR "*** vfs: kmem_cache_alloc failed for entry %d\n", i);
            vfs_free_memory();
            return -ENOMEM;
        }
    }

    int rc = register_filesystem(&vfs_type);
    if (rc)
    {
        printk(KERN_ERR "*** vfs: register_filesystem failed\n");
        vfs_free_memory();
        return rc;
    }
    printk(KERN_DEBUG "*** vfs: registered filesystem\n");

    return 0;
}

static void __exit vfs_exit(void)
{
    printk(KERN_DEBUG "*** vfs: exit\n");
    vfs_free_memory();
    int rc = unregister_filesystem(&vfs_type);
    if (rc)
        printk(KERN_ERR "*** vfs: unregister_filesystem failed\n");
    else
        printk(KERN_DEBUG "*** vfs: unregistered filesystem\n");
}

module_init(vfs_init);
module_exit(vfs_exit);
