备份超级块，在默认情况下

backup-super
                     mkfs.ocfs2,  by default, makes up to 6 backup copies of the super block at offsets 1G, 4G, 16G, 64G, 256G and 1T depending on the size of the volume.  This can be
                     useful in disaster recovery. This feature is fully compatible with all versions of the file system and generally should not be disabled.
              local  Create the file system as a local mount, so that it can be mounted without a cluster stack.

              sparse Enable support for sparse files. With this, OCFS2 can avoid allocating (and zeroing) data to fill holes. Turn this feature on if you can,  otherwise  extends  and
                     some writes might be less performant.

              unwritten
                     Enable  unwritten  extents support. With this turned on, an application can request that a range of clusters be pre-allocated within a file. OCFS2 will mark those
                     extents with a special flag so that expensive data zeroing doesn’t have to be performed. Reads and writes to a pre-allocated region act as reads and writes  to  a
                     hole, except a write will not fail due to lack of data allocation. This feature requires sparse file support to be turned on.

              inline-data
                     Enable  inline-data support. If this feature is turned on, OCFS2 will store small files and directories inside the inode block. Data is transparently moved out to
                     an extent when it no longer fits inside the inode block. In some cases, this can also make a positive impact on cold-cache directory and file operations.

              extended-slotmap
                     The slot-map is a hidden file on an OCFS2 fs which is used to map mounted nodes to system file resources. The extended slot map allows a larger range of  possible
                     node numbers, which is useful for userspace cluster stacks. If required, this feature is automatically turned on by mkfs.ocfs2.

              metaecc
                     Enables metadata checksums. With this enabled, the file system computes and stores the checksums in all metadata blocks. It also computes and stores an error cor-
                     rection code capable of fixing single bit errors.

              refcount
                     Enables creation of reference counted trees. With this enabled, the file system allows users to create inode-based snapshots and clones known as reflinks.

              xattr  Enable extended attributes support. With this enabled, users can attach name:value pairs to objects within the file system. In OCFS2, the names can  be  upto  255
                     bytes  in  length,  terminated  by the first NUL byte. While it is not required, printable names (ASCII) are recommended. The values can be upto 64KB of arbitrary
                     binary data. Attributes can be attached to all types of inodes: regular files, directories, symbolic links, device nodes, etc. This feature is required for  users
                     wanting to use extended security facilities like POSIX ACLs or SELinux.

              usrquota
                     Enable  user  quota  support. With this feature enabled, filesystem will track amount of space and number of inodes (files, directories, symbolic links) each user
                     owns. It is then possible to limit the maximum amount of space or inodes user can have. See a documentation of quota-tools package for more details.

              grpquota
                     Enable group quota support. With this feature enabled, filesystem will track amount of space and number of inodes (files, directories, symbolic links) each  group
                     owns. It is then possible to limit the maximum amount of space or inodes user can have. See a documentation of quota-tools package for more details.

              indexed-dirs
                     Enable  directory indexing support. With this feature enabled, the file system creates indexed tree for non-inline directory entries. For large scale directories,
                     directory entry lookup perfromance from the indexed tree is faster then from the legacy
                     directory blocks.
