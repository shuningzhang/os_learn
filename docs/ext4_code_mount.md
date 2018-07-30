
#接口入口
挂载文件系统的命令行格式为:
`mount -t EXT4 /dev/sdb /mnt/test_dir`

该命令会触发一个系统调用,具体函数为sys_mount,如下代码是sys_mount的实现代码(fs/namespace.c).由该代码参数可以看出主要是mount命令传入的参数.
```
2932 SYSCALL_DEFINE5(mount, char __user *, dev_name, char __user *, dir_name,
2933                 char __user *, type, unsigned long, flags, void __user *, data)
2934 {
2935         int ret;
2936         char *kernel_type;
2937         char *kernel_dev;
2938         unsigned long data_page;
2939 
2940         kernel_type = copy_mount_string(type);
2941         ret = PTR_ERR(kernel_type);
2942         if (IS_ERR(kernel_type))
2943                 goto out_type;
2944 
2945         kernel_dev = copy_mount_string(dev_name);
2946         ret = PTR_ERR(kernel_dev);
2947         if (IS_ERR(kernel_dev))
2948                 goto out_dev;
2949 
2950         ret = copy_mount_options(data, &data_page);
2951         if (ret < 0)
2952                 goto out_data;
2953 
2954         ret = do_mount(kernel_dev, dir_name, kernel_type, flags,
2955                 (void *) data_page);
2956 
2957         free_page(data_page);
2958 out_data:
2959         kfree(kernel_dev);
2960 out_dev:
2961         kfree(kernel_type);
2962 out_type:
2963         return ret;
2964 }

```

调用关系
```
sys_mount
  >do_mount
    >user_path
    >do_new_mount
      >get_fs_type
      >vfs_kern_mount
        >alloc_vfsmnt 返回一个mount类型指针
        >mount_fs (super.c) 返回dentry类型指针
          >type->mount
      >put_filesystem
      >do_add_mount
        >real_mount
        >graft_tree 
```


* * *

- - -

_ _ _
fs f 
_ _ _


该函数调用的核心是do_mount,
- - -
sys_mount->do_mount
- - -
```
2691 long do_mount(const char *dev_name, const char __user *dir_name,
2692                 const char *type_page, unsigned long flags, void *data_page)
2693 {
2694         struct path path;
2695         int retval = 0;
2696         int mnt_flags = 0;
2697 
2698         /* Discard magic */
2699         if ((flags & MS_MGC_MSK) == MS_MGC_VAL)
2700                 flags &= ~MS_MGC_MSK;
2701 
2702         /* Basic sanity checks */
2703         if (data_page)
2704                 ((char *)data_page)[PAGE_SIZE - 1] = 0;
2705 
2706         /* ... and get the mountpoint */
2707         retval = user_path(dir_name, &path);
2708         if (retval)
2709                 return retval;
2710 
2711         retval = security_sb_mount(dev_name, &path,
2712                                    type_page, flags, data_page);
2713         if (!retval && !may_mount())
2714                 retval = -EPERM;
2715         if (retval)
2716                 goto dput_out;
2717 
2718         /* Default to relatime unless overriden */
2719         if (!(flags & MS_NOATIME))
2720                 mnt_flags |= MNT_RELATIME;
2721 
2722         /* Separate the per-mountpoint flags */
2723         if (flags & MS_NOSUID)
2724                 mnt_flags |= MNT_NOSUID;
2725         if (flags & MS_NODEV)
2726                 mnt_flags |= MNT_NODEV;
2727         if (flags & MS_NOEXEC)
2728                 mnt_flags |= MNT_NOEXEC;
2729         if (flags & MS_NOATIME)
2730                 mnt_flags |= MNT_NOATIME;
2731         if (flags & MS_NODIRATIME)
2732                 mnt_flags |= MNT_NODIRATIME;
2733         if (flags & MS_STRICTATIME)
2734                 mnt_flags &= ~(MNT_RELATIME | MNT_NOATIME);
2735         if (flags & MS_RDONLY)
2736                 mnt_flags |= MNT_READONLY;
2737 
2738         /* The default atime for remount is preservation */
2739         if ((flags & MS_REMOUNT) &&
2740             ((flags & (MS_NOATIME | MS_NODIRATIME | MS_RELATIME |
2741                        MS_STRICTATIME)) == 0)) {
2742                 mnt_flags &= ~MNT_ATIME_MASK;
2743                 mnt_flags |= path.mnt->mnt_flags & MNT_ATIME_MASK;
2744         }
2745 
2746         flags &= ~(MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_ACTIVE | MS_BORN |
2747                    MS_NOATIME | MS_NODIRATIME | MS_RELATIME| MS_KERNMOUNT |
2748                    MS_STRICTATIME);
2749 
2750         if (flags & MS_REMOUNT)
2751                 retval = do_remount(&path, flags & ~MS_REMOUNT, mnt_flags,
2752                                     data_page);
2753         else if (flags & MS_BIND)
2754                 retval = do_loopback(&path, dev_name, flags & MS_REC);
2755         else if (flags & (MS_SHARED | MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE))
2756                 retval = do_change_type(&path, flags);
2757         else if (flags & MS_MOVE)
2758                 retval = do_move_mount(&path, dev_name);
2759         else
2760                 retval = do_new_mount(&path, type_page, flags, mnt_flags,
2761                                       dev_name, data_page);
2762 dput_out:
2763         path_put(&path);
2764         return retval;
2765 }
```




#结构体定义
fs/mount.h文件中对mount的定义

```
struct mount {
        struct hlist_node mnt_hash;
        struct mount *mnt_parent;
        struct dentry *mnt_mountpoint;
        struct vfsmount mnt;
        union {
                struct rcu_head mnt_rcu;
                struct llist_node mnt_llist;
        };
#ifdef CONFIG_SMP
        struct mnt_pcp __percpu *mnt_pcp;
#else
        int mnt_count;
        int mnt_writers;
#endif
        struct list_head mnt_mounts;    /* list of children, anchored here */
        struct list_head mnt_child;     /* and going through their mnt_child */
        struct list_head mnt_instance;  /* mount instance on sb->s_mounts */
        const char *mnt_devname;        /* Name of device e.g. /dev/dsk/hda1 */
        struct list_head mnt_list;
        struct list_head mnt_expire;    /* link in fs-specific expiry list */
        struct list_head mnt_share;     /* circular list of shared mounts */
        struct list_head mnt_slave_list;/* list of slave mounts */
        struct list_head mnt_slave;     /* slave list entry */
        struct mount *mnt_master;       /* slave is on master->mnt_slave_list */
        struct mnt_namespace *mnt_ns;   /* containing namespace */
        struct mountpoint *mnt_mp;      /* where is it mounted */
        struct hlist_node mnt_mp_list;  /* list mounts with the same mountpoint */
#ifdef CONFIG_FSNOTIFY
        struct hlist_head mnt_fsnotify_marks;
        __u32 mnt_fsnotify_mask;
#endif
        int mnt_id;                     /* mount identifier */
        int mnt_group_id;               /* peer group identifier */
        int mnt_expiry_mark;            /* true if marked for expiry */
        struct hlist_head mnt_pins;
        struct fs_pin mnt_umount;
        struct dentry *mnt_ex_mountpoint;
};
```

include/linux/dcache.h
```
108 struct dentry {
109         /* RCU lookup touched fields */
110         unsigned int d_flags;           /* protected by d_lock */
111         seqcount_t d_seq;               /* per dentry seqlock */
112         struct hlist_bl_node d_hash;    /* lookup hash list */
113         struct dentry *d_parent;        /* parent directory */
114         struct qstr d_name;
115         struct inode *d_inode;          /* Where the name belongs to - NULL is
116                                          * negative */
117         unsigned char d_iname[DNAME_INLINE_LEN];        /* small names */
118 
119         /* Ref lookup also touches following */
120         struct lockref d_lockref;       /* per-dentry lock and refcount */
121         const struct dentry_operations *d_op;
122         struct super_block *d_sb;       /* The root of the dentry tree */
123         unsigned long d_time;           /* used by d_revalidate */
124         void *d_fsdata;                 /* fs-specific data */
125 
126         struct list_head d_lru;         /* LRU list */
127         struct list_head d_child;       /* child of parent list */
128         struct list_head d_subdirs;     /* our children */
129         /*
130          * d_alias and d_rcu can share memory
131          */
132         union {
133                 struct hlist_node d_alias;      /* inode alias list */
134                 struct rcu_head d_rcu;
135         } d_u;
136 };

```

include/linux/mount.h
```
 66 struct vfsmount {
 67         struct dentry *mnt_root;        /* root of the mounted tree */
 68         struct super_block *mnt_sb;     /* pointer to superblock */
 69         int mnt_flags;
 70 };
```


include/linux/path.h
```
  7 struct path {
  8         struct vfsmount *mnt;
  9         struct dentry *dentry;
 10 };
 ```
