# 格式化文件系统

```
[root@ol6u9ext3 mnt]# mkfs.ext2 /dev/sdd
mke2fs 1.43-WIP (20-Jun-2013)
/dev/sdd is entire device, not just one partition!
无论如何也要继续? (y,n) y
文件系统标签=
OS type: Linux
块大小=4096 (log=2)
分块大小=4096 (log=2)
Stride=0 blocks, Stripe width=0 blocks
786432 inodes, 3145728 blocks
157286 blocks (5.00%) reserved for the super user
第一个数据块=0
Maximum filesystem blocks=3221225472
96 block groups
32768 blocks per group, 32768 fragments per group
8192 inodes per group
Superblock backups stored on blocks: 
	32768, 98304, 163840, 229376, 294912, 819200, 884736, 1605632, 2654208

Allocating group tables: 完成                            
正在写入inode表: 完成                            
Writing superblocks and filesystem accounting information: 完成 

```

# 超级块与磁盘布局



# 写数据流程

# 读数据流程

# 磁盘块分配

# 磁盘块回收


# 创建文件流程

# 删除文件流程

# 修改文件名

# 文件夹遍历

# 创建扩展属性

# 删除扩展属性

# 扩展属性遍历

# 锁机制


# 主要数据结构

