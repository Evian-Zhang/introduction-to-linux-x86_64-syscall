# `stat`, `fstat`, `lstat`, `newfstatat`, `statx`系统调用

## `stat`, `fstat`, `lstat`与`newfstatat`

### 系统调用号

`stat`为4，`fstat`为5，`lstat`为6，`newfstatat`为262。

### 函数签名

#### 内核接口

```c
asmlinkage long sys_newstat(const char __user *filename, struct stat __user *statbuf);
asmlinkage long sys_newfstat(unsigned int fd, struct stat __user *statbuf);
asmlinkage long sys_newlstat(const char __user *filename, struct stat __user *statbuf);
asmlinkage long sys_newfstatat(int dfd, const char __user *filename, struct stat __user *statbuf, int flag);
```

#### glibc封装

`stat`, `fstat`, `lstat`:

```c
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);
```

`newfstatat`:

```c
#include <fcntl.h>
#include <sys/stat.h>
int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags);
```

### 简介

这四个系统调用都和文件的状态信息有关，也就是和`struct stat`这个结构体密切相关。首先，我们来看看其定义（该结构体在不同的指令集、内核版本中都不尽相同，该结构体为当前Linux内核版本下的x86_64版本）：

```c
struct stat {
    dev_t st_dev;
    ino_t st_ino;
    nlink_t st_nlink;
    mode_t st_mode;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
#ifdef __USE_XOPEN2K8
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
#else
    time_t st_atime;
    unsigned long st_atimensec;
    time_t st_mtime;
    unsigned long st_mtimensec;
    time_t st_ctime;
    unsigned long st_ctimensec;
#endif
};
```

其中每个字段的含义如下：

* `st_dev`: 包含该文件的设备ID
* `st_ino`: inode数
* `st_nlink`: 该文件的硬链接数
* `st_mode`: 文件类型和模式。
    * 文件类型

        通过与位掩码`S_IFMT`相与可得到相应的文件类型：
        * `S_IFSOCK`: 套接字文件
        * `S_IFLINK`: 符号链接
        * `S_IFREG`: 常规文件
        * `S_IFBLK`: 块设备
        * `S_IFDIR`: 目录
        * `S_IFCHR`: 字符设备
        * `S_IFOFO`: FIFO（管道）
    * 文件模式

        通过与下列掩码相与可以得到相应的数据：
        * 特殊权限
            * `S_ISUID`: SUID位

                在大部分情况下，如果将一个可执行的二进制程序的该位设为1，则运行该二进制程序产生的进程的euid与该文件的uid相同。该进程拥有该文件属主的权限。

            * `S_ISGID`: SGID位

                在大部分情况下，如果将一个可执行的二进制程序的该位设为1，则运行该二进制程序产生的进程的egid与该文件的gid相同。该进程拥有该文件属组的权限。

                如果将一个目录的该位设为1，则表明在其中创建的所有文件的gid均与该目录相同，而不与创建该文件的进程的gid相同。

            * `S_ISVTX`: Sticky位

                如果将一个目录的该位设为1，则该目录中所有文件只能被该文件的属主、该目录的属主以及特权进程重命名或删除。

        * 所有者权限
            * `S_IRWXU`: 属主拥有读、写、执行权限
            * `S_IRUSR`: 属主拥有读权限
            * `S_IWUSR`: 属主拥有写权限
            * `S_IXUSR`: 属主拥有执行权限
        * 用户组权限
            * `S_IRWXG`: 属组拥有读、写、执行权限
            * `S_IRGRP`: 属组拥有读权限
            * `S_IWGRP`: 属组拥有写权限
            * `S_IXGRP`: 属组拥有执行权限
        * 其它用户权限
            * `S_IRWXO`: 他人拥有读、写、执行权限
            * `S_IROTH`: 他人拥有读权限
            * `S_IWOTH`: 他人拥有写权限
            * `S_IXOTH`: 他人拥有执行权限
* `st_uid`: 该文件的uid
* `st_gid`: 该文件的gid
* `st_rdev`: 如果该文件表示一个设备，则为该文件所表示的设备ID
* `st_size`: 文件大小（以字节计）

    如果该文件是一个符号链接，则表示其链接的源路径对应的字符串长度

* `st_blksize`: 使用高效文件IO时，推荐使用的块大小
* `st_blocks`: 分配给该文件的块数目（以512字节为一个单位）
* 时间戳
    * 首先，各个名称的意义：
        * `atime`: 最后访问时间
        * `mtime`: 最后修改时间
        * `ctime`: 文件状态最后修改时间
        * `mtime`与`ctime`的区别在于，前者是文件内容的修改，而后者则是文件对应inode的修改。如只修改`st_atime`字段，而不修改文件内容，则`mtime`不变，`ctime`发生改变。
    * 如果：
        * `_POSIX_C_SOURCE`宏定义为不小于200809L的值
        * `_XOPEN_SOURCE`定义为不小于700的值
        * `_BSD_SOURCE`被定义
        * `_SVID_SOURCE`被定义

        上述条件只需要满足任意一个，我们就可以通过`st_atime`或`st_atim.tv_sec`访问UNIX时间戳（以秒为单位），`st_atime.tv_nsec`访问其纳秒部分（不是总时长，而是除去秒部分，剩下的纳秒部分的大小）。其余几个时间也可以用类似的方式访问秒和纳秒部分。
    * 如果上面的条件都不满足，我们则可以通过`st_atime`访问UNIX时间戳（以秒为单位），`st_atimensec`访问其纳秒部分。
    * 也就是说，无论何种情况，我们都可以用`st_atime`访问以秒为单位的UNIX时间戳。

`stat`是根据文件路径获得相应的文件状态信息，但如果文件是一个符号链接，则会获得其指向的实际文件的信息。

`lstat`也是根据文件路径获得相应的文件信息，但如果文件是一个符号链接，则会获得链接本身的状态信息。

`fstat`是根据文件描述符获得相应的文件信息，行为和`stat`相同。

`fstatat`则是一个更普遍的接口，可以提供`stat`, `lstat`, `fstat`的功能。首先，`dirfd`与`pathname`的用法与`openat`相同，既可以用于绝对路径，也可以用于相对特定目录的相对路径，也可以用于相对当前目录的相对路径。其次，对于`flags`，其主要的标志位的功能有：

* 如果包含`AT_EMPTY_PATH`标志位，且`filename`是空字符串，则获得`dirfd`句柄对应的文件的状态信息。此时`dirfd`可以是任何文件类型，而不一定是目录。这种行为类似`fstat`。
* 如果包含`AT_SYMLINK_NOFOLLOW`标志位，且`dirfd`和`filename`组成的路径是一个符号链接，则获得链接本身的状态信息（相当于`lstat`）；如果不包含该标志位，则获得链接指向的实际文件的信息（相当于`stat`）。

### 实现

这四个系统调用的实现均位于`fs/stat.c`中。其实现的核心为位于`fs/stat.c`中的`vfs_getattr_nosec`函数：

```c
int vfs_getattr_nosec(const struct path *path, struct kstat *stat, u32 request_mask, unsigned int query_flags)
{
	struct inode *inode = d_backing_inode(path->dentry);

	memset(stat, 0, sizeof(*stat));
	stat->result_mask |= STATX_BASIC_STATS;
	request_mask &= STATX_ALL;
	query_flags &= KSTAT_QUERY_FLAGS;

	/* allow the fs to override these if it really wants to */
	if (IS_NOATIME(inode))
		stat->result_mask &= ~STATX_ATIME;
	if (IS_AUTOMOUNT(inode))
		stat->attributes |= STATX_ATTR_AUTOMOUNT;

	if (inode->i_op->getattr)
		return inode->i_op->getattr(path, stat, request_mask,
					    query_flags);

	generic_fillattr(inode, stat);
	return 0;
}
```

我们可以看到，其最关键的就在于`inode->i_op->getattr`。因此，其和`read`, `write`类似，也是通过函数指针实现多态的方式，获得相应的状态。

## `statx`

### 系统调用号

332

### 函数签名

#### 内核接口

```c
asmlinkage long sys_statx(int dfd, const char __user *path, unsigned flags, unsigned mask, struct statx __user *buffer);
```

#### glibc封装

```c
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
int statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf);
```

上述头文件是在`statx`的man手册里写的，但实际使用时似乎还应该包含`linux/stat.h`头文件。

### 简介

该系统调用的功能类似与`newfstatat`，但是，其提供的信息的类型是`struct statx`而不是`struct stat`，其包含的信息更多。

使用该函数的方式与`fstatat`类似，其中`mask`表示的是用户感兴趣的字段。也就是说，我们可以通过`mask`让返回的`statxbuf`只有部分字段有意义，其余不感兴趣的字段不需要内核来填充。其提供的标志位包括：

* `STATX_TYPE`: 得到`stx_mode & S_IFMT`
* `STATX_MODE`: 得到`stx_mode & ~S_IFMT`
* `STATX_NLINK`: 得到`stx_nlink`
* `STATX_UID`: 得到`stx_uid`
* `STATX_GID`: 得到`stx_gid`
* `STATX_ATIME`: 得到`stx_atime`
* `STATX_MTIME`: 得到`stx_mtime`
* `STATX_CTIME`: 得到`stx_ctime`
* `STATX_INO`: 得到`stx_ino`
* `STATX_SIZE`: 得到`stx_size`
* `STATX_BLOCKS`: 得到`stx_blocks`
* `STATX_BASIC_STATS`: 得到上述全部
* `STATX_BTIME`: 得到`stx_btime`
* `STATX_ALL`: 得到所有的字段

该结构体的内容是：

```c
struct statx {
	__u32	stx_mask;	/* What results were written [uncond] */
	__u32	stx_blksize;	/* Preferred general I/O size [uncond] */
	__u64	stx_attributes;	/* Flags conveying information about the file [uncond] */
	__u32	stx_nlink;	/* Number of hard links */
	__u32	stx_uid;	/* User ID of owner */
	__u32	stx_gid;	/* Group ID of owner */
	__u16	stx_mode;	/* File mode */
	__u16	__spare0[1];
	__u64	stx_ino;	/* Inode number */
	__u64	stx_size;	/* File size */
	__u64	stx_blocks;	/* Number of 512-byte blocks allocated */
	__u64	stx_attributes_mask; /* Mask to show what's supported in stx_attributes */
	struct statx_timestamp	stx_atime;	/* Last access time */
	struct statx_timestamp	stx_btime;	/* File creation time */
	struct statx_timestamp	stx_ctime;	/* Last attribute change time */
	struct statx_timestamp	stx_mtime;	/* Last data modification time */
	__u32	stx_rdev_major;	/* Device ID of special file [if bdev/cdev] */
	__u32	stx_rdev_minor;
	__u32	stx_dev_major;	/* ID of device containing file [uncond] */
	__u32	stx_dev_minor;
	__u64	__spare2[14];	/* Spare space for future expansion */
};
```

其与`struct stat`的主要区别在于：

* `stx_mask`: 即传入的`mask`参数，表示哪些字段有意义
* `stx_attributes`: 该文件更多的状态信息。其主要包含如下标志位：
    * `STATX_ATTR_IMMUTABLE`: 该文件不能被修改
    * `STATX_ATTR_APPEND`: 该文件只能以append模式写入，也就是说每次写入都必须在文件最后
    * `STATX_ATTR_DAX`: 该文件处于DAX状态（CPU直接访问）。TODO：进一步解释
* `stx_attributes_mask`: 掩码，用于表示`stx_attributes`的哪些标志位被设置了（有可能有的标志位没有被操作系统设置，而不是没有包含该标志位）

### 实现

其实现与`stat`等系统调用相同。内核会根据系统调用的类型，确定一个内部的掩码，根据掩码获得相应的文件信息。
