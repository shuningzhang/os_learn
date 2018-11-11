# 简介
本篇文章总结crash命令的基本用法。该命令用于对Linux内核panic的调试。

基本用法如下：
``


## Set命令
set [pid | taskp | [-c cpu] | -p] | [crash_variable [setting]] | -v
1、设置要显示的内容，内容一般以进程为单位。
Set pid 设置当前的内容为pid所代表的进程
Set taskp 设置当前的内容为十六制表示的taskp任务的内容
Set –p 设置当前的内容为panic任务的内容
Set -v 显示crash当前的内部变量
Set 不带参数，表示显示当前任务的内容
2、同时set命令也可以设置当前crash的内部变量
Set scroll on表示开启滚动条。
具体的内部变量可以通过set –v命令获得，也可以通过help set来查看帮助。

## Ascii
把一个十六进制表示的字符串转化成ascii表示的字符串
Ascii 不带参数则显示ascii码表
Ascii number number所代表的ascii字符串

## Struct
struct struct_name[.member[,member]][-o][-l offset][-rfu] [address | symbol]
[count | -c count]
显示结构体的具体内容（下面只介绍常用的，具体的可通过命令help struct查询）
注：如果crash关键字与name所表示的结构体名称不冲突，可以省略struct关键字。
Struct name 显示name所表示的结构体的具体结构
Struct name.member 显示name所表示的结构体中的member成员
Struct name –o 显示name所表示的结构体的具体结构，同时也显示每个成员的偏移量
注：如果crash关键字与name所表示的结构体名称不冲突，可以省略struct关键字。

Union
union union_name[.member[,member]] [-o][-l offset][-rfu] [address | symbol]
[count | -c count]
显示联合体的具体内容，用法与struct一致。

*
它是一个快捷键，用来取代struct和union。
Struct page == *page
Struct page == *page

P
p [-x|-d][-u] expression
Print的缩写，打印表达式的值。表达式可以为变量，也可以为结构体。
通过命令alias可以查看命令缩写的列表。
Px expression == p –x expression 以十六进制显示expression的值
Pd expression == p –d expression 以十进制显示expression的值
不加参数的print，则根据set设置来显示打印信息。

Whatis
whatis [struct | union | typedef | symbol] 
搜索数据或者类型的信息
参数可以是结构体的名称、联合体的名称、宏的名称或内核的符号。

Sym
sym [-l] | [-M] | [-m module] | [-p|-n] | [-q string] | [symbol | vaddr]
把一个标志符转换到它所对应的虚拟地址，或者把虚拟地址转换为它所对应的标志符。
Sym –l 列出所有的标志符及虚拟地址
Sym –M 列出模块标志符的集合
Sym –m module name 列表模块name的虚拟地址
Sym vaddr 显示虚拟地址addr所代表的标志
Sym symbol 显示symbol标志符所表示的虚拟地址
Sym –q string 搜索所有包含string的标志符及虚拟地址

Dis
dis [-r][-l][-u][-b [num]] [address | symbol | (expression)] [count]
disassemble的缩写。把一个命令或者函数分解成汇编代码。
Dis symbol 
Dis –l symbol

Bt
bt [-a|-g|-r|-t|-T|-l|-e|-E|-f|-F|-o|-O] [-R ref] [-I ip] [-S sp] [pid | task]
跟踪堆栈的信息。
Bt 无参数则显示当前任务的堆栈信息
Bt –a 以任务为单位，显示每个任务的堆栈信息
Bt –t 显示当前任务的堆栈中所有的文本标识符
Bt –f 显示当前任务的所有堆栈数据，通过用来检查每个函数的参数传递

Dev
dev [-i | -p]
显示数据关联着的块设备分配，包括端口使用、内存使用及PCI设备数据
Dev –I 显示I/O端口使用情况
Dev –p 显示PCI设备数据

Files
files [-l | -d dentry] | [-R reference] [pid | taskp]
显示某任务的打开文件的信息
Files 显示当前任务下所有打开文件的信息
File –l 显示被服务器锁住的文件的信息

Irq
irq [[[index ...] | -u] | -d | -b]
显示中断编号的所有信息
Irq 不加参数，则显示所有的中断
Irq index 显示中断编号为index的所有信息
Irq –u 仅仅显示正在使用的中断

Foreach
foreach [[pid | taskp | name | [kernel | user]] ...] command [flag] [argument]
跟C#中的foreach类似，为多任务准备的。它根据参数指定的任务中去查找command相关的内容。任务可以用pid、taskp、name来指定。如果未指定，则搜索所有的任务。形如：
Foreach bash task 表示搜索任务bash中的task相关数据。

当command为{bt,vm,task,files,net,set,sig,vtop}时，显示的内容与命令中的命令类似，只是加了foreach则显示所有任务，而不是单条任务。形如：
Foreach files 显示所有任务打开的文件

Runq
无参数。显示每个CPU运行队列中的任务。

Alias
alias [alias] [command string]
创建给定的命令的别名，如果未指定参数，则显示创建好的别名列表。
Command string可以是带各种参数的命令。

Mount
mount [-f] [-i] [-n pid|task] [vfsmount|superblock|devname|dirname|inode]
显示挂载的相关信息
Mount 不加参数，则显示所有已挂载的文件系统
Mount –f 显示每个挂载文件系统中已经打开的文件
Mount –I 显示每个挂载文件系统中的dirty inodes

Search
search [-s start] [ -[kKV] | -u | -p ] [-e end | -l length] [-m mask] -[cwh] value ...
搜索在给定范围的用户、内核虚拟内存或者物理内存。如果不指定-l length或-e end，则搜索虚拟内存或者物理内存的结尾。内存地址以十六进制表示。
-u 如果未指定start，则从当前任务的用户内存搜索指定的value
-k 如果未指定start，则从当前任务的内核内存搜索指定的value
-p 如果未指定start，则从当前任务的物理内存搜索指定的value
-c 后面则指定要搜索的字符串，这个搜索中很有用。

Vm
vm [-p | -v | -m | [-R reference] | [-f vm_flags]] [pid | taskp] ...
显示任务的基本虚拟内存信息。
-p 显示虚拟内存及转换后的物理内存信息

Net
net [-a] [[-s | -S] [-R ref] [pid | taskp]] [-n addr]
显示各种网络相关的数据
-a 显示ARP cache
-s 显示指定任务的网络信息
-S 与-s相似，但是显示的信息更为详细
该命令与foreach配合使用，能加快定位的速度。

Vtop
vtop [-c [pid | taskp]] [-u|-k] address ...
显示用户或内核虚拟内存所对应的物理内存。其中-u和-k分别表示用户空间和内核空间。

Ptov
ptov address ...
该命令与vtop相反。把物理内存转换成虚拟内存。


Btop
btop address ...
把一个十六进制表示的地址转换成它的分页号。

Ptob
ptob page_number ...
该命令与btop相反，是把一个分页号转换成地址。


Sig
sig [[-l] | [-s sigset]] | [-g] [pid | taskp] ...
显示一个或者多个任务的signal-handling数据
-l 列出信息的编号及名字
-g 显示指定任务线程组中所有的signal-handling数据

Waitq
waitq [ symbol ] | [ struct.member struct_addr ] | [ address ]
列出在等待队列中的所有任务。参数可以指定队列的名称、内存地址等。

Pte
pte contents ...
把一个十六进制表示的页表项转换为物理页地址和页的位设置

Swap
无参数。显示已经配置好的交换设备的信息。

Wr
wr [-u|-k|-p] [-8|-16|-32|-64] [address|symbol] value
根据参数指定的写内存。在定位系统出错的地方时，一般不使用该命令。

Eval
eval [-b][-l] (expression) | value
计算表达式的值，及把计算结果或者值显示为16、10、8和2进制。表达式可以有运算符，包括加减乘除移位等。
-b 统计2进制位数为1的索引编号。

List
list [[-o] offset] [-e end] [-s struct[.member[,member]]] [-H] start
显示链表的内容

Mach
mach [-cm]
显示机器的一些信息，如CPU主频等。
-c 显示每个CPU的结构体信息
-m 显示物理内存每段的映射

Log
log [-m]
显示内核的日志，以时间的先后顺序排列
-m 在每个消息前添加该消息的日志等级

Sys
sys [-c [name|number]] config
显示特殊系统的数据。不指定参数，则显示crash启动时打印的系统数据。
-c [name|number] 如果不指定参数，则显示所有的系统调用。否则搜索指定的系统调用。
Config 显示内核的配置。不过必须把CONFIG_IKCONFIG编进内核

Rd
rd [-dDsSupxmf][-8|-16|-32|-64][-o offs][-e addr] [address|symbol] [count]
显示指定内存的内容。缺少的输出格式是十六进制输出
-d 以十进制方式输出
-D 以十进制无符号输出
-8 只输出最后8位
-16 只输出最后16位
-32 只输出最后32位
-64 只输出最后64位
-o offs 开始地址的偏移量
-e addr 显示内存，直到到过地址addr为止
Address 开始的内存地址，以十六进制表示
Symbol 开始地址的标识符
Count 按多少位显示内存地址。如addr=1234，count=8，则显示34 12

Task
task [-R member[,member]] [pid | taskp] ...
显示指定内容或者进程的task_struct的内容。不指定参数则显示当前内容的task_struct的内容。
Pid 进程的pid
Taskp 十六进制表示的task_struct指针。
-R member 

Extend
extend [shared-object ...] | [-u [shared-object ...]]
动态装载或卸载crash额外的动态链接库。

Repeat
repeat [-seconds] command
每隔seconds重复一次命令command，无限期的执行下去。

Timer
无参数。按时间的先后顺序显示定时器队列的数据。

Gdb
gdb command ...
用GDB执行命令command。

# 使用实例


# 参考文献
1. [官方文档](http://people.redhat.com/anderson/crash_whitepaper/)