在介绍Linux的块子系统之前我们先看一张图，这张图来自thomas-krenn网站，大家可以从该网站找到各个版本的原图。这个图比较清晰的描述了一个IO从用户层到具体物理设备的可能情况。

![](./block_and_scsi/Linux-storage-stack-diagram_v4.10.png)

当文件系统层面调用submit_bio的时候，请求将被提交到块设备层。这里的块设备可能是一个物理块设备（例如磁盘或者优盘）或者虚拟块设备（比如MD设备）等。


设备探测

![](./block_and_scsi/probe_lun.png)

