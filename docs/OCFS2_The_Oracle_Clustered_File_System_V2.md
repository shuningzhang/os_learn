
> 本文原作者为Mark Fasheh，原文标题是OCFS2: The Oracle Clustered File System, Version 2。
> 翻译本文的目的在于学习OCFS2文件系统和英文，错误支出请指正。
> 译者：SunnyZhang

# Abstract(摘要)
This talk will review the various components of the OCFS2 stack, with a focus on the file system and its clustering aspects. OCFS2 extends many local file system features to the cluster, some of the more interesting of which are posix unlink semantics, data consistency,shared readable mmap, etc.

本文的将重新审视OCFS2栈的各个组件，重点关注文件系统和集群方面的内容。OCFS2将很多本地文件系统的特性集群化，很多更有趣的诸如posix兼容的unlink、数据一直想和可共享读的mmap等。

In order to support these features, OCFS2 logially separates cluster access into multiple layers. An overview of the low level DLM layer will be given. The higher level file system locking will be described in detail, including a walkthrough of inode locking and messaging for various operations.

为了支持这些特性，OCFS2将集群访问划分为逻辑的多层。