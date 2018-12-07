OCFS2/DesignDocs/DataInInode

OCFS2 Support for Data in Inode Blocks

Mark Fasheh

September 6, 2007

Goals / Requirements

We want to take advantage of Ocfs2's very large inodes by storing file and directory data inside of the same block as other inode meta data. This should improve performance of small files and directories with a small number of names in them. It will also reduce the disk usage of small files.

The implementation must also be flexible enough to allow for future sharing of unused inode space, most likely in the form of inline extended attributes.

Disk Structures

The superblock gets a new incompat bit:

/* Support for data packed into inode blocks */
#define OCFS2_FEATURE_INCOMPAT_INLINE_DATA      0x0040

The ocfs2_dinode gets a dynamic features field and a flag for data-in-inode:

/*
 * Flags on ocfs2_dinode.i_dyn_features
 *
 * These can change much more often than i_flags. When adding flags,
 * keep in mind that i_dyn_features is only 16 bits wide.
 */
#define OCFS2_INLINE_DATA_FL    (0x0001)        /* Data stored in inode block */
#define OCFS2_HAS_XATTR_FL      (0x0002)
#define OCFS2_INLINE_XATTR_FL   (0x0004)
#define OCFS2_INDEXED_DIR_FL    (0x0008)
struct ocfs2_dinode {
        ...
        __le16 i_dyn_features;
        ...
};

And the meta data lvb gets a mirror of that field:

struct ocfs2_meta_lvb {
        __u8         lvb_version;
        __u8         lvb_reserved0;
        __be16       lvb_idynfeatures;
        ...
};

A new structure, ocfs2_inline_data is added for storing inline data:

/*
 * Data-in-inode header. This is only used if i_flags has
 * OCFS2_INLINE_DATA_FL set.
 */
struct ocfs2_inline_data
{
/*00*/  __le16  id_count;       /* Number of bytes that can be used
                                 * for data, starting at id_data */
        __le16  id_reserved0;
        __le32  id_reserved1;
        __u8    id_data[0];     /* Start of user data */
};

And the ocfs2_dinode gets struct ocfs2_inline_data embedded in the id2 union:

struct ocfs2_dinode {
        ...
/*C0*/  union {
                struct ocfs2_super_block        i_super;
                struct ocfs2_local_alloc        i_lab;
                struct ocfs2_chain_list         i_chain;
                struct ocfs2_extent_list        i_list;
                struct ocfs2_truncate_log       i_dealloc;
                struct ocfs2_inline_data        i_data;
                __u8                            i_symlink[0];
        } id2;
/* Actual on-disk size is one block */
};

Managing Dynamic Inode Features

A flag on ocfs2_dinode is needed so that the code can determine when to use the inline data structure, as opposed to the extent list. In the future, it is anticipated that similar flags will have to be added for things like extended attributes, inline extended attributes, directory indexing and so on. The LVB structure will have to be capable of passing these flags around and they will need to be set and cleared automatically as meta data locks are taken.

ocfs2_dinode has only one flags field today, i_flags. It is typically used for flags that are rarely change. Most in fact, are only ever set by programs like mkfs.ocfs2 or tunefs.ocfs2. Examples of such flags are OCFS2_SYSTEM_FL, OCFS2_BITMAP_FL, and so on. The only ones that are manipulated by the file system, OCFS2_VALID_FL and OCFS2_ORPHANED_FL are only done at very specific, well defined moments in an inodes lifetime.

i_flags is never set or cleared from any lvb code or any generic update code (ocfs2_mark_inode_dirty()). In order to support a data-in-inode flag, we'd have to carefully mask out existing i_flags flags. Additionally, the LVB would be required to hold an additional 32 bits of information.

Instead of using i_flags, we create a new field, i_dyn_features. This way the code for manipulating the flags will be cleaner, and less likely to unintentionally corrupt a critical inode field. Since it would only be used for dynamic features, we can just use a 16 bit field. In the future, i_dyn_features can hold information relating to extended attributes.

The ocfs2_inline_data Structure

struct ocfs2_inline_data is embedded in the disk inode and is only used if OCFS2_INLINE_DATA_FL is set. Likewise, putting data inside an inode block requires that OCFS2_INLINE_DATA_FL gets set and struct ocfs2_inline_data be initialized.

Today, there are two fields in struct ocfs2_inline_data.

id_count
	

Describes the number of bytes which can be held inside of the inode.

id_data
	

Marks the beginning of the inode data. It is exactly id_count bytes in size.

In the future, id_count may be manipulated as extended attributes are stored/removed from the inode.

i_size is used to determine the end of the users data, starting at the beginning of id_data. All bytes in id_data beyond i_size but before id_count must remain zero'd.

i_blocks (memory only) and i_clusters for an inode with inline data are always zero.

High Level Strategy

Essentially what we have to do in order to make this work seamlessly is fool applications (and in some cases, the kernel) into thinking that inline data is just like any other. Data is "pushed" back out into an extent when the file system gets a request which is difficult or impossible to service with inline data. Directory and file data have slightly different requirements, and so are described in separate subsections.

File Inodes

For file inodes, we mirror info to/from the disk into unmapped pages via special inline-data handlers which can be called from Ocfs2 VFS callbacks such as ->readpage() or ->aio_write().

When we finally need to turn the inode into an extent list based one, the page is mapped to disk and adjacent pages within the cluster need to be zero'd and similarly mapped. data=ordered mode is respected during this process.

Strategy for specific file system operations is outlined in the table below.

File System Operation
	

Strategy for Existing Inline-data
	

New Strategy for Extents if applicable

Buffered Read (->readpage(), etc)
	

Copy from disk into page
	

NA

Buffered Write
	

If the write will still fit within id_count, mirror from the page onto disk. Otherwise, push out to extents.
	

If i_clusters and i_size are zero, and the resulting write would fit in the inode, we'll turn it into an inline-data inode.

O_DIRECT Read/Write
	

Fall back to buffered I/O
	

NA

MMap Read
	

Same as buffered read - we get this via ->readpage()
	

NA

MMap Write
	

Push out to an extent list on all mmap writes
	

NA

Extend (ftruncate(), etc)
	

If the new size is inside of id_count, just change i_size.
	

NA

Truncate/Punching Holes (ftruncate(), OCFS2_IOC_UNRESVSP*, etc)
	

Zero the area requested to be removed. Update i_size if the call is from ftruncate()
	

NA

Space Reservation (OCFS2_IOC_RESVSP*)
	

Push out to extents if the request is past id_count. Otherwise, nothing to do
	

NA

Directory Inodes

Directories are one area where I expect to see a significant speedup with inline data - a 4k file system can hold many directory entries in one block.

On a high level, directory inodes are simpler than file inodes - there's no mirroring that needs to happen inside of a page cache page so pushing out to extents is trivial. Locking is straightforward - just about all operations are protected via i_mutex.

Looking closer at the code however, it seems that many low level dirops are open coded, with some high level functions (ocfs2_rename() for example) understanding too much about the directory format. And all places assume that the dirents start at block offset zero and continue for blocksize bytes. The answer is to abstract things further out. Any work done now in that area will help us in the future when we begin looking at indexed directories.

Since directories always start with data, they will always start with OCFS2_INLINE_DATA_FL. This winds up saving us a bitmap lock since no data allocation is required for directory creation.

The code for expanding a directory from inline-data is structured so that the initial expansion will also guarantee room for the dirent to be inserted. This is because the dirent to be inserted might be larger than the space freed up from just expanding to one block. In that case, we'll want to expand to two blocks. Doing both blocks in one pass means that we can rely on our allocators ability to find adjacent clusters.

A table of top level functions which access directories will help to keep things in perspective.

Function Name
	

Access Type

ocfs2_readdir
	

Readdir

ocfs2_queue_orphans
	

Readdir

_ocfs2_get_system_file_inode
	

Lookup

ocfs2_lookup
	

Lookup

ocfs2_get_parent
	

Lookup

ocfs2_unlink
	

Lookup, Remove, Orphan Insert

ocfs2_rename
	

Lookup, Remove, Insert, Orphan Insert

ocfs2_mknod
	

Lookup, Insert, Create

ocfs2_symlink
	

Lookup, Insert

ocfs2_link
	

Lookup, Insert

ocfs2_delete_inode
	

Orphan Remove

ocfs2_orphan_add
	

Insert

ocfs2_orphan_del
	

Remove

Open Questions

Do we create new files with `OCFS2_INLINE_DATA_FL`?

Note that I literally mean "files" here - see above for why directories always start with inline data.

There's a couple of ways we can handle this. One thing I realized early is that we don't actually have 100% control over state transitions - that is, file write will always want to be able to turn an empty extent list into inline data. We can get into that situations from many paths. Tunefs.ocfs2 could set OCFS2_FEATURE_INCOMPAT_INLINE_DATA on a file system which has empty files. Also, on any file system, the user could truncate an inode to zero (the most common form of truncate) and we'd certainly want to turn that into inline-data on an appropriate write.

Right now, new inodes are still created with an empty extent list. Write will do the right thing when it see's them, and the performance cost to re-format that part of the inode is small. This has the advantage that the code to turn an inode into inline-data from write gets tested more often. Also, it's not uncommon that the 1st write to a file be larger than can fit inline.

Can we "defragment" an inline directory on the fly?

This would ensure that we always have optimal packing of dirents, thus preventing premature extent list expansion. The actual defrag code code be done trivially. The problem is that dirents would be moving around, which might mean that a concurrent readdir() can get duplicate entries. Maybe a scheme where we only defrag when there are no concurrent directory readers would work.

Locking

As a small refresher on how locks nest in Ocfs2:

i_mutex -> ocfs2_meta_lock() -> ip_alloc_sem -> ocfs2_data_lock()

Generally, locking stays the same. Initially, I thought we could avoid the data lock, but we still want to use it in for forcing page invalidation on other nodes. The locking is only really interesting when we're worried about pushing out to extents (or turning an empty extent list into inline data).

As usual, mmap makes things tricky. It can't take i_mutex, so most real work has to be done holding ip_alloc_sem.

Here are some rules which mostly apply to files, as directory locking is much less complicated.

    i_mutex is sufficient for spot-checking inline-data state. The inode will never have inline-data added if you hold i_mutex, however it might be pushed out to extents if ocfs2_page_mkwrite() beats you to ip_alloc_sem.

    To create an inline data section, or go from inline data to extents the process must hold a write lock on ip_alloc_sem. This is the only node-local protection that mmap can count on.

    Reading inline data requires only the cluster locks and a read on ip_alloc_sem 

ChangeLog

    Sept. 20, 2007: Completed description, based on completed patches - Mark Fasheh
    Sept 6, 2007: Begin complete re-write based on my prototype - Mark Fasheh
    Apr 12, 2007: Sprinkled some comments - Sunil Mushran
    Dec 4, 2006: Andrew Beekhof also do the same work, so I quit
    Dec 3, 2006: Add mount option, dlm LVB structure
    Nov 30, 2006: Initial edition 

##　OCFS2/DesignDocs/InodeAllocationStrategy

OCFS2 Inode Allocation Strategy

Mark Fasheh <mfasheh at suse dot com>

Initial revision based on a conversation with Jan Kara <jack at suse dot cz>

April 16, 2008

This document describes the result of conversations various Ocfs2 developers and testers have had regarding inode allocation.

Inode Alloc Problem Areas

During a recent conversation, Jan Kara and I identified two interesting topics related to inode allocation.

New inodes being spread out

The core problem here is that the inode block search looks for the "emptiest" inode group to allocate from. So if an inode alloc file has many equally (or almost equally) empty groups, new inodes will tend to get spread out amongst them, which in turn can put them all over the disk. This is undesirable because directory operations on conceptually "nearby" inodes force a large number of seeks.

The easiset way to see this is to start with a fresh file system and create a large number of inodes in it - enough to force the allocation of a few inode groups. At this point, things are actually reasonably distributed because the allocator started in an empty state. If the inodes are removed and then recreated however, they'll get spread out amongst the existing inode groups as the allocator cycles through them since each group keeps geting slightly less "empty" than the next.

A fix is to change inode group selection criteria from "emptiest" to something that makes more sense for inode allocation.

Most file systems try to group inodes with their parent directory. As described by Jan, ext3 takes the following steps:

<jack> ext3 code does:
<jack> start looking at parent's group, take the first group which satisfies these conditions:
<jack> it has at least 'min_blocks' free, it has at least 'min_inodes' free, it doesn't have too many directories.
<jack> where 'too many directories' mean - number_of_directories_on_fs / number_of_groups + inodes_per_group / 16
<jack> And then there's fallback if no group satisfies above requirements.
<jack> In the group, we just take the first free inode.

Unfortunately, things are a bit trickier for Ocfs2 as it's access to allocators is transient in nature. So building up global inode / directory counts would require cluster messaging.

Also, Ocfs2 allocators grow on demand so there are times when the allocator will have very little free space for optimization. We could, at a future date though change the group add heuristics to pre-emptively allocate extra groups to allow for better inode placement.

That said, Ocfs2 can try a simplified version of the standard scheme which should produce some favorable results:

In core directory inodes are given some extra fields, which records the last used allocation group and allocation slot.

When allocating an inode:

    If last_used_group is set and we use the same allocation slot, search that group first. If no free block is found, fall back to less optimal search, and set last_used_group and last_used_slot to the result.
    If last_used_group is not set and the parent directory is in our allocator, start looking for free inodes in the chunk of the parent directory. Again, fall back to less optimal search if no free inodes are found. Always set last_used_group to the result. 

One thing to watch out for is that we'll lose information when the fs is unmounted, or when the inode is pruned from memory. If this is proven to be a real problem, we could always adding a disk field to record this information.

Another consideration is that we might want to try locking the parent directories allocator even if it's in another slot. This has the downside of incurring cluster locking overhead, but the upside is that placement won't be thwarted simply by the directory having been created on another node.I don't think this is good for us. With the above "last_used_group" solution, only the 1st inode may be discontiguous with the parent, and from then on, we can use the saved group and all the following inodes will be contiguous and it is OK enough for us.

A last thought - if the parent group doesn't work for any reason and we don't have a last_used_group set, we could also try allocating from the same group as an inode whose directory data is adjacent to the one being inserted.This idea may be good. But I think we could defer it when index dir is committed to the kernel.

Inode Group Allocation

This topic requires much more exploration before we do anything concrete.

The idea here is that we might want to consider more explicit placement of inode groups. Right now they're allocated from the local alloc file, which may or may not be a bad thing.

For instance, keeping inode groups close to each other might help readdir+stat, which we're currently weak on. The downside of course is that data for those inodes might be on a different region of the disk [question - how well do we do in this regard today?]. An offset to that downside could be inline-data which wouldn't require seeks anyway for small files.

To allocate inode groups so that they are closer to each other:

    Inode group allocs use the global bitmap directly
    We try to allocate in same cluster group as last time, by remembering which one was last allocated from
    To pick initial cluster group, try for the emptiest one that the local alloc is not using 

Of course, we might decide that it's better to go the other way - explicitly spread out cluster groups. Other algorithms could be designed around that concept. 


OCFS2/DesignDocs/IndexedDirectories

Ocfs2 Indexed Directories Design Doc

DRAFT

Mark Fasheh <mfasheh at suse dot com>

August 7, 2008

Introduction

This document describes the type of changes required so that Ocfs2 can support directory indexing. We want this so that the file system can support fast lookup of directory entries. At the same time, we want to:

    Maintain POSIX compliance of directory operations. Typically this means that a readdir(2) operation can't see the same entry twice (so inserts/unlinks aren't allowed to move entries around). telldir(2) is also a problem in that it's cookie is very small.

    Avoid some of the performance problems that htree has. The problem with htree is that dirents are read (via readdir(2)) in hash order. For readdir+stat` workloads, this winds up doing a large amount of random I/O. 

Secondary goals are that we increase performance of create and unlink. We don't want it to take O(N) time to insert a record, when lookup of that record is already O(1). Rename is nasty, but to the disk format, it's essentially a compound operation made up of unlinks and creates so it needs not be heavily considered here.

Another primary goal is that of backwards compatibility. We want a running file system to understand both directory formats. This goal is very easy however, since we already have per-inode features fields (i_dyn_features) which we can use to indicate whether a directory is indexed or not. It is not necessary that the file system be able to convert a directory from one format to another.

Structure Review

/*
 * On-disk directory entry structure for OCFS2
 *
 * Packed as this structure could be accessed unaligned on 64-bit platforms
 */
struct ocfs2_dir_entry {
/*00*/  __le64   inode;                  /* Inode number */
        __le16   rec_len;                /* Directory entry length */
        __u8    name_len;               /* Name length */
        __u8    file_type;
/*0C*/  char    name[OCFS2_MAX_FILENAME_LEN];   /* File name */
/* Actual on-disk length specified by rec_len */
} __attribute__ ((packed));

Some notes about the existing directory format:

    It is fully POSIX compliant - dirents never move, even to create free space. Also, a directory is never truncated, unless deleted.

    readdir+stat will generally be reading inodes in insert order. For Ocfs2, this is typically close to block number order.

    '.' and '..' entries have a fixed offset. I think that some software might expect that. It should be trivial for us to fake this if we need to. 

Design

Two Btrees

I propose that we keep two btrees per directory, an unindexed tree and a name-indexed tree.

Unindexed Tree

The unindexed tree would be nearly identical to the existing Ocfs2 directory allocation btree, where the leaves are blocks of sequential ocfs2_dir_entry structures. We would use this tree for readdir. That way, we preserve our insert-order stats and POSIX compliance. The tree would be stored in exactly the same place in the inode as it is today. This would also be the only place where we store full dirents, so rebuilding a corrupted directory (from fsck) would involve a walk of this tree and re-insert of found records into the indexed tree (discussed later).

The only addition to our standard directory tree would be a small record at the beginning of each block which would facilitate our ability to find free space in the tree. Additionally, we take advantage of the opportunity to provide a per-block checksum. We can actually hide this in a fake directory entry record if need be. Changes to the existing code would be minimal.

/*
 * Per-block record for the unindexed directory btree. This is carefully
 * crafted so that the rec_len and name_len records of an ocfs2_dir_entry are
 * mirrored. That way, the directory manipulation code needs a minimal amount
 * of update.
 *
 * NOTE: Keep this structure aligned to a multiple of 4 bytes.
 */
struct ocfs2_dir_block_hdr {
/*00*/  __u8    db_signature[8];
        __le16  db_compat_rec_len;      /* Backwards compatible with
                                           ocfs2_dir_entry. */
        __u8    db_compat_name_len;     /* Always zero. Was name_len */
        __u8    db_free_list;           /* What free space list is this dir
                                         * block on. Set to
                                         * OCFS2_IDIR_FREE_SPACE_HEADS when the
                                         * block is completely full */
        __le32  db_reserved1;
        __le64  db_free_prev;           /* Physical block pointers comprising a
        __le64  db_free_next;            * doubly linked free space list */
        __le64  db_csum;
};

Name Indexed Tree

We can keep a name-indexed btree rooted in a separate block from the directory inode. Lookup would exclusively use this tree to find dirents.

Each record in the indexed tree provides just enough information so that we can find the full dirent in the unindexed tree. This allows us to keep a much more compact tree, with fixed-length records as opposed to the unindexed tree whose records can be large and of multiple sizes. The distinction helps - for most block sizes, collisions should not be a worry. Fixed-length records also make manipulation of indexed dir blocks more straight forward. The expense of course is a couple of additinoal reads, however even with that read, we stay well within our limit of O(1). In order to reduce the number of leaves to search when multiple records with the same hash are found, we also store the entries name_len field.

Since the size of indexed dirents is so small, I don't think we need to use the same bucket scheme as DesignDocs/IndexedEATrees. It should be sufficient to allow that the hashed name determines the logical offset of which indexed tree block to search. We'd have room for over 200 collisions per block on a 4k blocksize file system, over 100 collisions on 2k blocksize, and so on.

Aside from lookup, unlink performance is also improved - since we know the name to unlink, we can hash it, look it up in the index and know exactly where the corresponding dirent is.

The only operation which does not get a performance increase from this is insert of directory entries. While the indexed tree insert is O(1), an insert into the unindexed tree still requires an O(N) search of it's leaf blocks for free space. To solve this problem, I propose that we link unindexed leaf blocks together into lists according to their largest contiguous amount of free space. The next section describes this in more detail.

#define OCFS2_IDIR_FREE_SPACE_HEADS     16

/*
 * A directory indexing block. Each indexed directory has one of these,
 * pointed to by ocfs2_dinode.
 *
 * This block stores an indexed btree root, and a set of free space
 * start-of-list pointers.
 *
 * NOTE: Directory indexing is not supported on 512 byte block size file
 * systems 
 */
struct ocfs2_idir_block {
       __u8     idx_signature[8];
       __le64   idx_csum;
       __le64   idx_fs_generation;
       __le64   idx_blkno;
       __le64   idx_reserved1[2];
       __le32   idx_reserved2;
       __le16   idx_suballoc_slot;
       __le16   idx_suballoc_bit;
       __le64   idx_free_space[OCFS2_IDIR_FREE_SPACE_HEADS];
       struct ocfs2_extent_list idx_list;
};

/*
 * A directory entry in the indexed tree. We don't store the full name here,
 * but instead provide a pointer to the full dirent in the unindexed tree.
 *
 * We also store name_len here so as to reduce the number of leaf blocks we
 * need to search in case of collisions.
 */
struct ocfs2_idir_dirent {
       __le32   hash;           /* Result of name hash */
       __u8     name_len;       /* Length of entry name */
       __u8     reserved1;
       __le16   reserved2;
       __le64   dirent_off;     /* Logical offset of dirent in unindexed tree */
};

/*
 * The header of a leaf block in the indexed tree.
 */
struct ocfs2_idir_block_hdr {
       __u8     ih_signature[8];
       __le64   ih_csum;
       __le64   ih_reserved1;
       __le32   ih_reserved2;
       __le16   ih_count;       /* Maximum number of entries possible in ih_entries */
       __le16   ih_num_used;    /* Current number of ih_entries entries */
       struct ocfs2_idir_dirent ih_entries[0];  /* Indexed dir entries in a packed array of length ih_num_used */
};

Free Space List

The free space lists help us find which unindexed dir block to insert a given dirent into. Each dir block can be part of a single list, or none at all if it is full. Which list a dir block is stored in depends on the size of the contiguous free space found in that block.

Unlink, insert and rename may have to move a leaf block between lists. To make this easier, the entries are doubly linked.

So that we can find the 1st entries in each list, an array of free space list headers are stored in the block holding the indexed tree root. Each header is associated with a certain size. It's list entries are comprised of blocks which have at least that large a hole for dirents, but still less than the size of the next header.

TODO: The list sizes breakdown I have right now is only to illustrate the point. We can do better if we take average dirent sizes into account and distribute our list sizes accordingly.

To look for a leaf block with a certain amount of free space, F, we select the highest ordered list whose entries are equal to or less than F. If we search the entire list and don't find anything, we can move up to the next sized list and be guaranteed to find enough space in it's first entry. If there are no larger list entries, then we know a new leaf block has to be added.

While this won't give us better than O(N) in the worst case, it should greatly improve our chances of finding free space in the average case. In particular, directories whose inner leaves are almost or completely full (this is common) will not require a full scan.

TODO: The list sizes breakdown I have right now is only to illustrate the point. We can do better if we take average dirent size into account.

#define OCFS2_DIR_MIN_REC_LEN           16  /*  OCFS2_DIR_REC_LEN(1) */
/* Maximum rec_len can be almost block-size as dirents cover the entire block. OCFS2_DIR_REC_LEN(OCFS2_MAX_FILENAME_LEN) however, is 268 */

static inline int ocfs2_dir_block_which_list(struct ocfs2_dir_block_hdr *hdr)
{
        if (hdr->db_free_max == 0)
           return -1;

        return ocfs2_idir_free_size_to_idx(le16_to_cpu(hdr->db_free_max));
}

/*
 * Given an index to our free space lists, return the minimum allocation size
 * which all blocks in this list are guaranteed to support.
 */
static inline unsigned int ocfs2_idir_free_idx_to_min_size(unsigned int idx)
{
        BUG_ON(idx >= OCFS2_IDIR_FREE_SPACE_HEADS);
        return (idx + 1) * 16;
}

/*
 * Given a rec len, return the index of which free space list to search.
 */
static inline unsigned int ocfs2_idir_free_size_to_idx(unsigned int rec_len)
{
        unsigned int idx = 0;
        unsigned int bytes;

        for (idx = (OCFS2_IDIR_FREE_SPACE_HEADS - 1); idx >= 0; idx--) {
            bytes = ocfs2_idir_free_idx_to_min_size(idx);
            if (rec_len >= bytes)
               break;
        }

        return idx;
}

Open Questions

How is POSIX readdir affected if a directory goes from inline to indexed?

The insertion of a block header for that 1st block technically changes the logical location of our dirents. In theory then, a readdir might see a dirent twice, if it races at exactly the right moment. What do we do about this?

    Nothing, and just call it a rare race.
    Waste space in the inline directory (undesirable) and keep a placeholder block header there.
    Move the block header to the back of the block. Can a full inline dir make this impossible though? Also, we can't easily change the size of the header if we do this.

    Fake the offsets used in readdir / telldir for inline dirs by adding sizeof(struct ocfs2_dir_block_hdr) if it's inline. 

I like the last solution best, but it requires us guaranteeing that an inline directory will never grow to anything other than an indexed one. I'm not sure why a user wouldn't want their directory to be indexed, but stranger things have happened. Of course, if we separate out the block header into it's own feature then we can always insert the block header into non-indexed directories too.

Do we use logical or physical block pointers?

    logical is cleaner
    physical provides better performance 

I think actually, that the free space list and list heads should be physical block pointers since we could be traversing them a lot. The dirent pointers in the indexed items can be logical though.

Absolutely physical structure should be physical block pointers. That's the free space list and list heads. For the dirent pointers... I don't like having extra reads, but somehow logical feels better. I like having an absolute linkage between index entry and dirent (byte offset is directly to the dirent). That said, a physical block offset and scanning the dirblock for the entry works just fine and is fewer reads. Is that indecisive enough? Basically, I'm trying to come up with a reason why either method would be better/worse when doing some future change. -- Joel

Well, we can directly address the dirent, regardless of whether we use logical or physical addresses. Since existing directory allocation doesn't change in the same way that file allocation could, it might make the most sense to use physical addresses in the indexed dirent too, based only on the performance gain. --Mark

Can we optimize lookup of "medium" size directories

I think the lookup code can use i_size to determine the maximum number of blocks it would search, had the directory not been indexed. If that number is less than or equal to the minimum number of blocks required to check the index tree, we can ignore the index altogether.

For example, say we have an index btree with only one level. In that case, the number of reads (after the inode has been read) is 2 - one for the root block, and one for the leaf. If the inodes size is less than or equal to 2 blocks, it might make sense to just search the unindexed tree. If we take block contiguousness and readahead into account, we might be able to optimize further. If clustersize is 16K, blocksize is 4K, readahead size is 16K, and i_size is one cluster or less, we might as well just pull in the entire leaf cluster in one I/O.

Another thing we can consider, is storing the index inline in the root index block until it gets large enough that a btree is necessary. This is similar to how we store EA blocks.

What size hash to use

We're using 32 bits as described above. There's a couple of considerations. Firstly, is 32 bits a large enough hash? It probably is, considering that we can have a fairly large number of collisions within a block. However, it still might be a good idea to increase the size of our hash so that the high 32 bits addresses the exact cluster in our btree, while the lower bits help us located the exact block within that cluster. That way, we can use the entire address space available to us for cluster sizes that are greater than block size. Since largest clustersize is 1meg (20 bits) and smallest blocksize is 512 bytes (9 bits), we'd need to use another 11 bits (probably just rounded to 16) for a 48 bit hash. Obviously, which parts of that are used depends on the clustersize / blocksize combination.

How do we fill up indexed tree leaves before rebalancing?

Obviously, we don't want to allocate a new leaf block for every insert. To get around this, we'll store entries in the empty leaf whose cpos is closest to but still less than the entries hash value. When a leaf fills up, we allocate a new one and rebalance by moving half the entries to the new leaf. Entries whose hash equals a the cpos of the leaf never move. When we've filled up a leaf with identical hash values, we -ENOSPC.

File systems where cluster size is different from block size pose a problem though. Which block do we chose (within the cluster) to store an entry? We'd like to spread them out within the cluster, so that we don't overload a given block, forcing early rebalance. This is actually related to the previous question about hash size. If we can assume a hash size large enough to also include block offset within the cluster, then the block offset from beginning of cluster to store in is: largehash & ~(clustersize_bits - blocksize_bits). So, expanding this:

logical block of hash = ((upper 32 bits of hash) << (clustersize_bits - blocksize_bits) + (hash & ~(clustersize_bits - blocksize_bits)

Of course, what we actually do is use the (upper 32 bits of hash) value to find the closest matching logical cluster (cpos), and then use the (hash & ~(clustersize_bits - blocksize_bits) result to find a block within that cluster.

Do hard links require any special considerations

I don't believe so. The unindexed tree can hold multiple hard links just fine. The indexed tree is name-indexed, so the likelyhood of collisions is low.

Should tunefs.ocfs2 convert old directories when turning on this feature?

Probably not as it's expensive. Obviously turning off the feature requires conversion of indexed directories, but that should be as easy as deleting the indexed tree and clearing the 1st dirent in each block. We could always provide a separate tunefs command (or standalone tool) to indexed an existing directory.

I think this is a good scheme. -- Joel

You know, we could even do most of a dir conversion in userspace by just hard linking files from the old dir into a new one. We could us mtime/ctime to detect changes that might have happened during the linking. --Mark

TODO

    pseudo code for insert
    pseudo code for unlink
    pseudo code for rename?
    tools changes needed
        tunefs.ocfs2
        mkfs.ocfs2
        libocfs2
        fsck.ocfs2 
       
       
OCFS2/DesignDocs/IndexedEATrees

OCFS2 Indexed Btrees for Extended Attributes

Mark Fasheh <mark dot fasheh at oracle dot com>

December 7, 2007

Required Reading

This document references the extended attributes design document heavily. In particular, familiarity with struct ocfs2_xattr_header and struct ocfs2_xattr_entry is required.

Introduction

The extended attributes design document describes an excellent method for storing small (in-inode) to medium (EA Block) numbers of extended attributes. Support for large numbers of attributes is touched upon by referencing a name indexed tree, but no specific description for how such a tree would work is given. With this document, I attempt to flesh out a more detailed design of a Btree indexed by EA name hashes.

To be clear, this isn't about support for large individual EA's - that's handled already in the orignal EA document by allowing for a value btree. This document is specifically about handlings large numbers of EA's. In many ways, this is more important. The number of applications using extended attributes is growing steadily and it's not out of the question to think that we would need to store more than one blocks worth of attributes today.

Primary Goals / Requirements

    Support for a large number of extended attributes
    Performant lookup of attribute names. 

One comment on the number of attributes we should be targeting. On one hand, I believe we shouldn't limit the total number of attributes. On the other hand, I'm much more concerned with the performance of hundreds (maybe thousands?) of EA's, than I am with "tens of thousands".

Overall with regards to performance, we want the same as any Ocfs2 features we've designed before - good performance with excellent core design that our customers will appreciate. It doesn't have to be the absolute fastest on the block, especially if that's at the expense of readability or mintainability. It should be "up there" though, and I don't think that's particularly beyond us.

Secondary Goals

    Avoid creating a completely new disk structure (High Priority). 

At first glance, this seems to mean that we don't want to put a full blown new set of structures on disk, and while that's certainly part of it, there's much more. Specifically, I feel that we should avoid a completely new set of algorithms and design approaches. For example - at the worst case an indexed btree might require some new disk structures and perhaps even a seperate btree implementation, it's still a btree with tradeoffs and space/performance considerations that we're used to.

    Make re-use of existing btree code (Medium-High Priority) 

If this just seems impossible, then we can fall back to re-implementing the parts of the code which differ too greatly. At any rate, if we have to re-implement some of the insert code, perhaps we could make it generic enough to enable the next goal. Hopefully things like hole-punching (for deleting a tree) could be mostly re-used though.

    Design with a close eye towards indexed directories (Medium Priority) 

This really shouldn't be too hard as I believe indexed directories are actually simpler (particularly in leaf format) than an EA tree, but if it starts to get tough, we can just worry about indexed directories when we get to them.

Existing Ocfs2 Btree Structures

The ocfs2 btree manipulation code is very specific about what a tree should look like.

    One extents logical offset plus length never overlaps with the logical offset of the extent next to it.
    Interior nodes are never sparse, each extent covers theoretical range which the lower nodes have (cpos always is what the lowest cpos below is) 

struct ocfs2_extent_rec {
/*00*/  __le32 e_cpos;          /* Offset into the file, in clusters */
        union {
                __le32 e_int_clusters; /* Clusters covered by all children */
                struct {
                        __le16 e_leaf_clusters; /* Clusters covered by this
                                                   extent */
                        __u8 e_reserved1;
                        __u8 e_flags; /* Extent flags */
                };
        };
        __le64 e_blkno;         /* Physical disk offset, in blocks */
/*10*/
};

Extents are allocated as clusters, which can range from 4k to 1M. Ocfs2 block sizes range from 512 bytes to 4k.

Design

We use e_cpos to store the smallest hash value within an extent. The range of hashes in an extent can then be calculated by comparing with the next extent record. e_leaf_clusters, then still only refers to the actual on-disk allocation.

Extents within a btree are broken up into equal sized 'buckets'. Hash values are distributed in sort order throughout the non-empty buckets in the extent. This should make a binary search (starting at the 1st bucket) simple. While the hash range of any given bucket can change as the extent is inserted into, we do not allow for overlapping ranges between buckets. The maximum number of collisions we will allow for then is one bucket's worth. If a set of EA's collide so much that they fill up the bucket, any subsequent attempts to insert another EA with the same hash will fail with ENOSPC.

Maximum size of an extent will be limited as time-to-search will degrade as the extent grows.

Open Question: As it stands, bucket range isn't actually written to a field anywhere and it's implicitly determined by the first and last values within the bucket. Should we bother writing range to disk? If so, how and where?

If bucket size is chosen so that it's never larger than cluster size AND we don't allow more than one buckets worth of collisons, then we can never break the "no overlapping extents" rule of the btree code.

My hope is that breaking the extents up into buckets will reduce the scope of most operations (inserts, removals) down to a manageable size. Searching should be only minimally worse than if we had a whole-extent size ordered list.

A superblock value can record the bucket size. I propose however, that for now we choose 4k as the bucket size for all file systems, regardless of other fs parameters. 4k is the maximum size which will automatically work with every cluster size and we're favoring 4k clusters very strongly so if something doesn't work with that sized buckets, we'll be in major trouble.

Bucket Format

Internally, each bucket is an ocfs2_xattr_header. The list, insert, and remove operations within a bucket are identical to those in a single external EA block except the data might span multiple fs blocks.

There's no need to worry about the le16 xh_count field in ocfs2_xattr_header. Doing the math with a minimum xattr_entry size of 16 bytes: 65535 * 16 = 1048576 which means we're safe until we want larger than 1M buckets.

Finding an EA

An EA is found by hashing it's name and searching an ocfs2 btree using e_cpos as the index to an extent which may contain the EA. If the exact cpos isn't found, then the next smallest value is used. For the purposes of the search, length fields are ignored. They only refer to the on-disk size of the extent, not the hash range. The leftmost extent records e_cpos value is always zero.

Once an extent is found, a binary search across the set of non-empty buckets is performed. The search can skip around the buckets until it either finds the value it's looking for, a set of equal hash value records whose names don't match, or a pair of adjacent records with values less than and greater than the key.

Inserts / Splits

An new extent is formatted as a series of empty buckets. Buckets are filled from left to right, starting with the first bucket in the extent. Each time a bucket is filled, the set of nonempty buckets is extended and the range of hash values are redistributed evenly through all buckets in the extent (the buckets are rebalanced). We continue this until all buckets within an extent are full or almost full.

We can store the current number of contiguous, non-empty buckets by storing it in the 16 bit xh_reserved1 field.

Once all buckets in an extent are full, we need to add another extent. Things are a bit different if the new extent is contiguous or not. If it's contiguous, we just continue the process of rebalancing the extent buckets. If the extent isn't contiguous, we pick a new cpos for it and move all EA's which now fit into the new one. The new cpos is chosen so as to be in the middle of the range that the existing extent covers: new_cpos = cpos + (highest_hash - lowest_hash) / 2

Open Question: We need some (hopefully simple) heuristics on when we consider an extent "full" and in need of being split.

Conflicts

One of the biggest questions I have about this is "how many collisions are reasonable?".

We can calculate several things here. Lets assume a bucket size of 4096 bytes. Removing 8 bytes of overhead for the top of ocfs2_xattr_header gives us 4088 bytes for storing EA's.

Let's assume that EA values are all ocfs2_xattr_value_root structures and we allow for two extents in each one. That gives us 16 bytes of overhead, 16 bytes for the ocfs2_extent_list, and 16 bytes per ocfs2_extent_rec, so a total of 64 bytes.

Structure
	

Bytes Required

ocfs2_xattr_header overhead
	

8 Bytes

ocfs2_xattr_entry
	

16 Bytes

ocfs2_xattr_value_root Overhead
	

16 Bytes

ocfs2_extent_list with two extents
	

48 Bytes (16 + 2x16)

Of course, the name is the most variable part of this.

Open Question: What is the average length of an EA name?

#define XATTR_NAME_MAX   255

Name Length
	

Per EA Overhead
	

# of EA's that fit in 4088 bytes

255
	

335
	

12

128
	

208
	

19

96
	

176
	

23

64
	

144
	

28

32
	

112
	

36

16
	

96
	

42

8
	

88
	

46

Item Removal

When the last item in a bucket is removed, we re-balance all the buckets in that extent.

Space Optimization Questions

The maximum inline value should be limited to 80 bytes, the size of a 2 extent struct ocfs2_xattr_tree_root.

Should we drop the whole ocfs2_xattr_value_root thing in the indexed tree and just use a single block pointer to a tree root? The block pointer would really suck for "medium" sized EA's.

Hashing Algorithm

    need a random seed in super block which is never printed on console
    TEA by default
    Should allow for different hash types via superblock parameter 
## 参考文献
https://oss.oracle.com/osswiki/OCFS2.html
https://oss.oracle.com/projects/ocfs2/documentation/DesignDocs/OldCompleted/