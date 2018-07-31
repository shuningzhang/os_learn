

# 函数主要调用流程


打开文件函数调用主要流程.
```
sys_open
  >do_sys_open
    >getname
    >get_unused_fd_flags
    >do_filp_open
      >path_openat
        >get_empty_filp
        >path_init
        >do_last
        >may_follow_link
        >follow_link
        >do_last
```

查找路径函数调用主要流程.
```
user_path
  >user_path_at
    >user_path_at_empty
      >getname_flags
      >filename_lookup
        >path_lookupat
          >path_init
          >lookup_last
          >may_follow_link
          >follow_link
          >lookup_last
          >complete_walk
          >d_can_lookup
```


# 代码分析

## 系统调用


fs/open.c
```
1026 SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
1027 {
1028         if (force_o_largefile())
1029                 flags |= O_LARGEFILE;
1030 
1031         return do_sys_open(AT_FDCWD, filename, flags, mode);
1032 }
```


fs/open.c
```
 998 long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
 999 {
1000         struct open_flags op;
1001         int fd = build_open_flags(flags, mode, &op);
1002         struct filename *tmp;
1003 
1004         if (fd)
1005                 return fd;
1006 
1007         tmp = getname(filename);
1008         if (IS_ERR(tmp))
1009                 return PTR_ERR(tmp);
1010 
1011         fd = get_unused_fd_flags(flags);
1012         if (fd >= 0) {
1013                 struct file *f = do_filp_open(dfd, tmp, &op);
1014                 if (IS_ERR(f)) {
1015                         put_unused_fd(fd);
1016                         fd = PTR_ERR(f);
1017                 } else {
1018                         fsnotify_open(f);
1019                         fd_install(fd, f);
1020                 }
1021         }
1022         putname(tmp);
1023         return fd;
1024 }
```
该函数实现了具体的文件打开过程,大致流程如下:
* 1007行将用户空间的路径名称拷贝到内核空间
* 1011行从当前进程找到一个可用的文件描述符
* 1013行,**do_filp_open**函数是核心,进行文件的实际打开动作.
* 1019行,函数**fd_install**对文件描述符和打开的文件结构进行关联

通过以上流程,在用户态就可以通过文件描述符/句柄访问具体的文件.

下面分析**do_filp_open**函数,代码如下:
- - -
**fs/namei.c**
- - - 
```
3388 struct file *do_filp_open(int dfd, struct filename *pathname,
3389                 const struct open_flags *op)
3390 {
3391         struct nameidata nd;
3392         int flags = op->lookup_flags;
3393         struct file *filp;
3394 
3395         filp = path_openat(dfd, pathname, &nd, op, flags | LOOKUP_RCU);
3396         if (unlikely(filp == ERR_PTR(-ECHILD)))
3397                 filp = path_openat(dfd, pathname, &nd, op, flags);
3398         if (unlikely(filp == ERR_PTR(-ESTALE)))
3399                 filp = path_openat(dfd, pathname, &nd, op, flags | LOOKUP_REVAL);
3400         return filp;
3401 }
```


**fs/namei.c**
- - - 
```
3325 static struct file *path_openat(int dfd, struct filename *pathname,
3326                 struct nameidata *nd, const struct open_flags *op, int flags)
3327 {
3328         struct file *file;
3329         struct path path;
3330         int opened = 0;
3331         int error;
3332 
3333         file = get_empty_filp();
3334         if (IS_ERR(file))
3335                 return file;
3336 
3337         file->f_flags = op->open_flag;
3338 
3339         if (unlikely(file->f_flags & __O_TMPFILE)) {
3340                 error = do_tmpfile(dfd, pathname, nd, flags, op, file, &opened);
3341                 goto out2;
3342         }
3343 
3344         error = path_init(dfd, pathname, flags, nd);
3345         if (unlikely(error))
3346                 goto out;
3347 
3348         error = do_last(nd, &path, file, op, &opened, pathname);
3349         while (unlikely(error > 0)) { /* trailing symlink */
3350                 struct path link = path;
3351                 void *cookie;
3352                 if (!(nd->flags & LOOKUP_FOLLOW)) {
3353                         path_put_conditional(&path, nd);
3354                         path_put(&nd->path);
3355                         error = -ELOOP;
3356                         break;
3357                 }
3358                 error = may_follow_link(&link, nd);
3359                 if (unlikely(error))
3360                         break;
3361                 nd->flags |= LOOKUP_PARENT;
3362                 nd->flags &= ~(LOOKUP_OPEN|LOOKUP_CREATE|LOOKUP_EXCL);
3363                 error = follow_link(&link, nd, &cookie);
3364                 if (unlikely(error))
3365                         break;
3366                 error = do_last(nd, &path, file, op, &opened, pathname);
3367                 put_link(nd, &link, cookie);
3368         }
3369 out:
3370         path_cleanup(nd);
3371 out2:
3372         if (!(opened & FILE_OPENED)) {
3373                 BUG_ON(!error);
3374                 put_filp(file);
3375         }
3376         if (unlikely(error)) {
3377                 if (error == -EOPENSTALE) {
3378                         if (flags & LOOKUP_RCU)
3379                                 error = -ECHILD;
3380                         else
3381                                 error = -ESTALE;
3382                 }
3383                 file = ERR_PTR(error);
3384         }
3385         return file;
3386 }

```

* 3333行获取一个file结构体
* 


