## 概述
前文已经大概介绍过OCFS2的[部署和应用场景](http://www.itworld123.com/2018/11/17/ocfs2%e9%9b%86%e7%be%a4%e6%96%87%e4%bb%b6%e7%b3%bb%e7%bb%9f%e5%8f%8a%e7%8e%af%e5%a2%83%e6%90%ad%e5%bb%ba/)，本文及后续文章重点介绍OCFS2文件系统的具体实现。为了便于后续代码的理解，本文首先介绍一下该文件系统关键数据的磁盘布局情况。

> 本文介绍基于Linux4.1.12内核，其它版本内核可能稍有不同，但不影响理解。

## 整体磁盘布局
如下图OCFS2文件系统与Ext4文件系统类似，将磁盘划分为若干组，Ext4文件系统叫块组（block group），这里成为簇组（cluster group），虽然概念不同，但大体用途基本一致。

![磁盘布局总图](http://github.itworld123.com/linux/fs/ocfs2_disk_layout.png)


