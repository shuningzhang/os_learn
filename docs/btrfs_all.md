# 格式化
## 单磁盘默认参数格式化
在该种场景并不组建任何RAID，与其它文件系统相同，采用默认参数进行格式化。此时btrfs文件系统的元数据采用DUP方式存放，也就是元数据会存放多分，从而放置出现元数据损坏，数据丢失的情况。

[root@ol6u9ext3 btrfs_ana]# mkfs.btrfs /dev/sde 
btrfs-progs v4.2.2
See http://btrfs.wiki.kernel.org for more information.

WARNING: The skinny-metadata mkfs default feature will work with the current kernel, but it is not compatible with older kernels supported on this OS. You can disable it with -O ^skinny-metadata option in mkfs.btrfs
WARNING: The extref mkfs default feature will work with the current kernel, but it is not compatible with older kernels supported on this OS. You can disable it with -O ^extref option in mkfs.btrfs
Label:              (null)
UUID:               788d7460-d5c8-4399-93d3-89fb769bdfac
Node size:          16384
Sector size:        4096
Filesystem size:    12.00GiB
Block group profiles:
  Data:             single            8.00MiB
  Metadata:         DUP               1.01GiB
  System:           DUP              12.00MiB
SSD detected:       no
Incompat features:  extref, skinny-metadata
Number of devices:  1
Devices:
   ID        SIZE  PATH
    1    12.00GiB  /dev/sde


## 2个磁盘的RAID0
由2个硬盘组成RAID0构建文件系统，元数据和数据都采用RAID0的模式。此时两者是平铺在2块磁盘上的。

```
[root@ol6u9ext3 btrfs_ana]# mkfs.btrfs -m raid0 -d raid0 /dev/sdc /dev/sde -f
btrfs-progs v4.2.2
See http://btrfs.wiki.kernel.org for more information.

WARNING: The skinny-metadata mkfs default feature will work with the current kernel, but it is not compatible with older kernels supported on this OS. You can disable it with -O ^skinny-metadata option in mkfs.btrfs
WARNING: The extref mkfs default feature will work with the current kernel, but it is not compatible with older kernels supported on this OS. You can disable it with -O ^extref option in mkfs.btrfs
Label:              (null)
UUID:               1e3cc0c5-2049-4a1e-9eec-c6942accdcb0
Node size:          16384
Sector size:        4096
Filesystem size:    24.00GiB
Block group profiles:
  Data:             RAID0             2.01GiB
  Metadata:         RAID0             2.01GiB
  System:           RAID0            20.00MiB
SSD detected:       no
Incompat features:  extref, skinny-metadata
Number of devices:  2
Devices:
   ID        SIZE  PATH
    1    12.00GiB  /dev/sdc
    2    12.00GiB  /dev/sde
```

#磁盘数据布局
每一个btrfs文件系统有一些树的根组成，一个刚刚格式化的文件系统包含如下根：
- 树根树
- 分配extent的树
- 缺省子卷的树

树根树记录了扩展树的根块以及每个子卷和快照树的根块和名称。当有事务提交是，根块的指针将会被事务更新为新的值。此时该树新的根块将被记录在FS的超级块中。
The tree of tree roots records the root block for the extent tree and the root blocks and names for each subvolume and snapshot tree. As transactions commit, the root block pointers are updated in this tree to reference the new roots created by the transaction, and then the new root block of this tree is recorded in the FS super block.

在文件系统中树根树好像是其它树的文件夹，而且它有该文件系统的记录着所有快照和子卷名称的文件夹项。在数根树中，每一个快照和子卷都有一个objectid，并且最少有一个对应的btrfs_root_item结构体。树中的目录项建立了快照和子卷名称与这些根项（btrfs_root_item）的关联关系。由于每次事务提交的时候根项Key都会被更新，因此目录项的generation的值是(u64)-1，这样可以使查找代码找到最近的可用根。

The tree of tree roots acts as a directory of all the other trees on the filesystem, and it has directory items recording the names of all snapshots and subvolumes in the FS. Each snapshot or subvolume has an objectid in the tree of tree roots, and at least one corresponding struct btrfs_root_item. Directory items in the tree map names of snapshots and subvolumes to these root items. Because the root item key is updated with every transaction commit, the directory items reference a generation number of (u64)-1, which tells the lookup code to find the most recent root available.

extent树用于在设备上管理已经分配的空间。剩余空间能够被划分为多个extent树，这样可以减少锁碰撞，而且为不同的空间提供不同的分配策略。
The extent trees are used to manage allocated space on the devices. The space available can be divided between a number of extent trees to reduce lock contention and give different allocation policies to different block ranges.

如图1描绘了一个树根的集合。超级块指向根树，根树又指向extent树和子卷树。根树也有一个文件夹来映射子卷名称和在根树中的btrfs_root_items结构体。本文件系统有一个名为default的子卷（该子卷在格式化是创建）和一个该子卷的名为snap的快照（由管理员在稍晚的时候创建的）。本例中，该子卷在快照创建后没有发生任何变化，因此两者的指针是指向同一颗树的根。

The diagram below depicts a collection of tree roots. The super block points to the root tree, and the root tree points to the extent trees and subvolumes. The root tree also has a directory to map subvolume names to struct btrfs_root_items in the root tree. This filesystem has one subvolume named 'default' (created by mkfs), and one snapshot of 'default' named 'snap' (created by the admin some time later). In this example, 'default' has not changed since the snapshot was created and so both point tree to the same root block on disk. 
![图1 ](./btrfs_all/btrfs_layout_tl.png)


FS Tree 管理文件相关的元数据，如 inode，dir等； Chunk tree管理设备，每一个磁盘设备都在Chunk Tree中有一个item；Extent Tree管理磁盘空间分配，btrfs每分配一段磁盘空间，便将该磁盘空间的信息插入到Extent tree。查询Extent Tree将得到空闲的磁盘空间信息；checksum Tree 保存数据块的校验和；Tree of tree root保存很多 BTree 的根节点。比如用户每建立一个快照，btrfs 便会创建一个FS Tree。


既然说到了Btree，就不能不提btrfs中的一些Btree设施，先来看下extent_buffer。顾名思义，即是extent在内存中的缓冲，它是btrfs文件系统磁盘空间管理的核心，btrfs通过btree来管理各种元数据，比如inode、目录项，等。这些B+树的每一个节点（包括叶子节点和上层节点）都存储在一个单位的extent中，每次要读取元数据或者要向磁盘写入元数据，则通常先先将数据读入extent_buffer或者向extent_buffe写入数据。

The Btrfs btree provides a generic facility to store a variety of data types. Internally it only knows about three data structures: keys, items, and a block header: 

![](./btrfs_all/tree_ds.png)
Upper nodes of the trees contain only [ key, block pointer ] pairs. Tree leaves are broken up into two sections that grow toward each other. Leaves have an array of fixed sized items, and an area where item data is stored. The offset and size fields in the item indicate where in the leaf the item data can be found. Example: 

![](./btrfs_all/Leaf-structure.png)
项数据的大小是可变的，各种文件系统数据结构被定义为不同类型的项数据。struct btrfs_disk_key中的type字段指示存储在项中的数据类型。


块头包含块内容的校验和、拥有该块的文件系统的UUID、树中该块的级别以及该块应该所在的块号。这些字段允许在读取数据时验证元数据的内容。指向btree块的所有内容还存储它期望该块具有的生成字段。这允许BTRFS检测介质上的虚写或错位写入。


较低节点的校验和不存储在节点指针中，以简化FS写回代码。生成号将在块插入btree时知道，但校验和仅在将块写入磁盘之前计算。使用生成将允许BTRFS检测幻象写入，而不必在每次更新较低节点校验和时查找和更新较高节点。


生成字段与分配块的事务ID相对应，该ID使增量备份变得容易，并由copy-on-write事务子系统使用。


#文件
占用空间小于一个叶子节点磁盘空间的小文件。此时，key的偏移是在本文件中的以字节为单位的偏移，而结构体btrfs_item中的size域则指示了有多少数据被存储。

Small files that occupy less than one leaf block may be packed into the btree inside the extent item. In this case the key offset is the byte offset of the data in the file, and the size field of struct btrfs_item indicates how much data is stored. There may be more than one of these per file.

大文件将以extent的模式进行存储。结构体btrfs_file_extent_item中记录着extent的generation数、  。extent同时存储着逻辑偏移和被该extent使用的块的数量。这样可以使btrfs在出现对一个extent中间位置进行改写时不用读取旧的数据。例如，向一个128MB的extent中写入1MB的数据，此时将产生3个extent记录。
[ old extent: bytes 0-64MB ], [ new extent 1MB ], [ old extent: bytes 65MB – 128MB]

Larger files are stored in extents. struct btrfs_file_extent_item records a generation number for the extent and a [ disk block, disk num blocks ] pair to record the area of disk corresponding to the file. Extents also store the logical offset and the number of blocks used by this extent record into the extent on disk. This allows Btrfs to satisfy a rewrite into the middle of an extent without having to read the old file data first. For example, writing 1MB into the middle of a existing 128MB extent may result in three extent records: 

占用少于一个叶块的小文件可以打包到扩展项内的btree中。在这种情况下，键偏移量是文件中数据的字节偏移量，结构btrfs_项的大小字段指示存储了多少数据。每个文件可能不止一个。


较大的文件存储在扩展数据块中。struct btrfs_file_extent_item记录扩展数据块的生成号和一对[disk block，disk num blocks]记录文件对应的磁盘区域。扩展数据块还将逻辑偏移量和此扩展数据块记录使用的块数存储到磁盘上的扩展数据块中。这使得btrfs可以在不需要先读取旧文件数据的情况下，将重写操作满足到数据块的中间。例如，将1MB写入现有128MB数据块的中间可能会导致三个数据块记录：

# 参考文献
https://btrfs.wiki.kernel.org/index.php/Data_Structures#CHUNK_ITEM_.28e4.29
https://btrfs.wiki.kernel.org/index.php/Btrfs_design
https://btrfs.wiki.kernel.org/index.php/On-disk_Format