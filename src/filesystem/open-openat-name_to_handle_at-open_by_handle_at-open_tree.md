# `open`, `openat`, `name_to_handle_at`, `open_by_handle_at`, `open_tree`系统调用

## `open`与`openat`

### 系统调用号

`open`为2，`openat`为257。

### 函数原型

#### 内核接口

```c
asmlinkage long sys_open(const char __user *filename, int flags, umode_t mode);
asmlinkage long sys_openat(int dfd, const char __user *filename, int flags, umode_t mode);
```

#### glibc封装

```c
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
int open(const char *pathname, int flags);
int open(const char *pathname, int flags, mode_t mode);
int openat(int dirfd, const char *pathname, int flags);
int openat(int dirfd, const char *pathname, int flags, mode_t mode);
```

### 简介

我们知道，绝大多数文件相关的系统调用都是直接操作文件描述符（file descriptor），而`open`和`openat`这两个系统调用是一种创建文件描述符的方式。`open`系统调用将打开路径为`filename`的文件，而`openat`则将打开相对描述符为`dirfd`的目录，路径为`filename`的文件。

详细来说，`open`和`openat`的行为是

* `filename`是绝对路径
  * `open`打开位于`filename`的文件
  * `openat`打开位于`filename`的文件，忽略`dirfd`
* `filename`是相对路径
  * `open`打开相对于当前目录，路径为`filename`的文件
  * `openat`打开相对于`dirfd`对应的目录，路径为`filename`的文件；若`dirfd`是定义在`fcntl.h`中的宏`AT_FDCWD`，则打开相对于当前目录，路径为`filename`的文件。

接着，是“怎么打开”的问题。`open`和`openat`的参数`flags`, `mode`控制了打开文件的行为（`mode`详情请见`creat`系统调用。TODO：增加超链接）。

#### flags

用于打开文件的标志均定义在`fcntl.h`头文件中。

##### 文件访问模式标志（file access mode flag）

* `O_RDONLY`

    以只读方式打开。创建的文件描述符不可写。

* `O_WDONLY`

    以只写方式打开。创建的文件描述符不可读。

* `O_RDWD`

    以读写方式打开。创建的文件描述符既可读也可写。

* `O_EXEC`

    以只执行方式打开。创建的文件描述符只可以被执行。只能用于非目录路径。

* `O_SEARCH`
    
	以只搜索方式打开。创建的文件描述符值可以被用于搜索。只能用于目录路径。

POSIX标准要求在打开文件时，必须且只能使用上述标志位中的一个。而glibc的封装则要求在打开文件时，必须且只能使用前三个标志位（只读、只写、读写）中的一个。

##### 文件创建标志（file creation flag）

文件创建标志控制的是`open`和`openat`在打开文件时的行为。部分比较常见的标志位有：

* `O_CLOEXEC`
    * 文件描述符将在调用`exec`函数族时关闭。
    * 我们知道，当一个Linux进程使用`fork`创建子进程后，父进程原有的文件描述符也会复制给子进程。而常见的模式是在`fork`之后使用`exec`函数族替换当前进程空间。此时，由于替换前的所有变量都不会被继承，所以文件描述符将丢失，而丢失之后就无法关闭相应的文件描述符，造成泄露。如[以下代码](https://github.com/Evian-Zhang/introduction-to-linux-x86_64-syscall/tree/master/codes/open-cloexec)：
      ```c
      #include <fcntl.h>
      #include <unistd.h>

      int main() {
          int fd = open("./text.txt", O_RDONLY);
          if (fork() == 0) {
              // child process
              char *const argv[] = {"./child", NULL};
              execve("./child", argv, NULL); // fd left opened
          } else {
              // parent process
              sleep(30);
          }
  
          return 0;
      }
      ```
      其中`./child`在启动30秒后会自动退出。
	
      在启动这个程序之后，我们使用`ps -aux | grep child`找到`child`对应的进程ID，然后使用
        ```shell
        readlink /proc/xxx/fd/yyy
        ```
      查看，其中`xxx`为进程ID，`yyy`是`fd`中的任意一个文件。我们调查`fd`中的所有文件，一定能发现一个文件描述符对应`text.txt`。也就是说，在执行`execve`之后，子进程始终保持着`text.txt`的描述符，且没有任何方法关闭它。
    * 解决这个问题的方法一般有两种：
        * 在`fork`之后，`execve`之前使用`close`关闭所有文件描述符。但是如果该进程在此之前创建了许多文件描述符，在这里就很容易漏掉，也不易于维护。
        * 在使用`open`创建文件描述符时，加入`O_CLOEXEC`标志位：
          ```c
          int fd = open("./text.txt", O_RDONLY | O_CLOEXEC);
          ```
          通过这种方法，在子进程使用`execve`时，文件描述符会自动关闭。
* `O_CREAT`
    * 当`filename`路径不存在时，创建相应的文件。
    * 使用此标志时，`mode`参数将作为创建的文件的文件模式标志位。详情请见`creat`系统调用。TODO: 增加超链接
* `O_EXCL`
    * 该标志位一般会与`O_CREAT`搭配使用。
    * 如果`filename`路径存在相应的文件（包括符号链接），则`open`会失败。
* `O_DIRECTORY`
    * 如果`filename`路径不是一个目录，则失败。
    * 这个标志位是用来替代`opendir`函数的。TODO: 解释其受拒绝服务攻击的原理。
* `O_TRUNC`
    * 如果`filename`路径存在相应的文件，且以写的方式打开（即`O_WDONLY`或`O_RDWD`），那么将文件内容清空。

##### 文件状态标志（file status flag）

文件状态标志控制的是打开文件后的后续IO操作。

* `O_APPEND`
    * 使用此标志位时，在后续每一次`write`操作前，会将文件偏移移至文件末尾。（详情请见[write](./write-pwrite64-writev-pwritev-pwritev2.md)）。
* `O_ASYNC`
    * 使用信号驱动的IO。后续的IO操作将立即返回，同时在IO操作完成时发出相应的信号。
    * 这种方式在异步IO模式中较少使用，主要由于这种基于中断的信号处理机制比系统调用的耗费更大，并且无法处理多个文件描述符同时完成IO操作。参考[What's the difference between async and nonblocking in unix socket?](https://stackoverflow.com/a/6260132/10005095)。
    * 对正常文件的描述符无效，对套接字等文件描述符有效。
* `O_NONBLOCK`
    * 后续的IO操作立即返回，而不是等IO操作完成后返回。
    * 对正常文件的描述符无效，对套接字等文件描述符有效。
	* 对于以此种方式打开的文件，后续的`read`和`write`操作可能会产生特殊的错误——`EAGAIN`（对于套接字文件还可能产生`EWOULDBLOCK`）。

	    这种错误的含义是接下来的读取或写入会阻塞，常见的原因可能是已经读取完毕了，或者写满了。比如说，当客户端发送的数据被服务器端全部读取之后，再次对以非阻塞形式打开的套接字文件进行`read`操作，就会返回`EAGAIN`或`EWOULDBLOCK`错误。

* `O_SYNC`与`O_DSYNC`
    * 使用`O_SYNC`时，每一次`write`操作结束前，都会将文件内容和元信息写入相应的硬件。
    * 使用`O_DSYNC`时，每一次`write`操作结束前，都会将文件内容写入相应的硬件（不保证元信息）。
    * 这两种方法可以看作是在每一次`write`操作后使用`fsync`。
* `O_PATH`
    * 仅以文件描述符层次打开相应的文件。
	* 我们使用`open`和`openat`打开文件通常有两个目的：一是在文件系统中找到相应的文件，二是打开文件对其内容进行查看或修改。如果传入`O_PATH`标志位，则只执行第一个目的，不仅耗费更低，同时所需要的权限也更少。
	* 通过`O_PATH`打开的文件描述符可以传递给`close`, `fchdir`, `fstat`等只在文件层面进行的操作，而不能传递给`read`, `write`等需要对文件内容进行查看或修改的操作。

#### 其他注意点

此外，还有一些需要注意的。

在新的Linux内核（版本不低于2.26）中，glibc的封装`open`底层调用的是`openat`系统调用而不是`open`系统调用（`dirfd`为`AT_FDCWD`）。我们可以在glibc源码的`sysdeps/unix/sysv/linux/open.c`中看到：

```c
int
__libc_open (const char *file, int oflag, ...)
{
  int mode = 0;

  if (__OPEN_NEEDS_MODE (oflag))
    {
      va_list arg;
      va_start (arg, oflag);
      mode = va_arg (arg, int);
      va_end (arg);
    }

  return SYSCALL_CANCEL (openat, AT_FDCWD, file, oflag, mode);
}
```

`open`的glibc封装实际上就是系统调用`openat(AT_FDCWD, file, oflag, mode)`。

`open`和`openat`返回的是文件描述符（file descriptor），在Linux内核中，还有一个概念为文件描述（file description）。操作系统会维护一张全局的表，记录所有打开的文件的信息，如文件偏移、打开文件的状态标志等。这张全局的表的表项即为文件描述。而文件描述符则是对文件描述的引用。

每一次成功调用`open`和`openat`，都会在文件描述表中创建一个新的表项，返回的文件描述符即是对该表项的引用。而我们常见的`dup`, `fork`等复制的文件描述符，则会指向同一个文件描述。

文件描述创建之后，不会随着文件路径的修改而修改。也就是说，当我们通过`open`打开了某个特定路径下的文件，然后我们将该文件移动到别的路径上，我们后续的`read`, `write`等操作仍能成功。

### 用法

我们在使用`open`和`openat`时，可以有如下的思考顺序：

1. 打开文件的路径是绝对路径还是相对路径
    * 绝对路径使用`open`和`openat`都可以
    * 对于相对路径而言，如果相对于当前目录，则可以使用`open`，但大部分情况而言还是`openat`适用性更广（相对于当前目录可以传递`AT_FDCWD`给`dirfd`参数）
2. 打开文件是否需要读、写
	* 只需要读，`flags`加入标志位`O_RDONLY`
	* 只需要写，`flags`加入标志位`O_WDONLY`
	* 既需要读，又需要写，`flags`加入标志位`O_RDWD`
3. 对于可能会产生子进程并使用`exec`函数族的程序，`flags`加入标志位`O_CLOEXEC`
4. 如果需要对文件进行写入：
	* 如果需要在写之前清空文件内容，`flags`加入标志位`O_TRUNC`
	* 如果需要在文件末尾追加，`flags`加入标志位`O_APPEND`
	* 如果文件不存在的时候需要创建文件，`flags`加入标志位`O_CREAT`，并且传递相应的文件模式标志位给`mode`

以下几种都是合理的使用方式：

```c
int fd1 = open(filename, O_RDONLY);
int fd2 = open(filename, O_RDWD | O_APPEND);
int fd3 = open(filename, O_WDONLY | O_CLOEXEC | O_TRUNC);
```

### 实现

`open`和`openat`的实现均位于`fs/open.c`文件中，与其相关的函数是`do_sys_open`:

```c
long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
{
	struct open_flags op;
	int fd = build_open_flags(flags, mode, &op);
	struct filename *tmp;

	if (fd)
		return fd;

	tmp = getname(filename);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);

	fd = get_unused_fd_flags(flags);
	if (fd >= 0) {
		struct file *f = do_filp_open(dfd, tmp, &op);
		if (IS_ERR(f)) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else {
			fsnotify_open(f);
			fd_install(fd, f);
		}
	}
	putname(tmp);
	return fd;
}
```

由其实现可知，无论路径是否一样，`flag`是否相同，`open`总会使用新的文件描述符。也就是说：

```c
int a = open("./text.txt", O_RDONLY);
int b = open("./text.txt", O_RDONLY);
```

尽管调用参数一样，`a`和`b`依然是不同的。

此外，这个函数调用了`do_filp_open`函数作为真正的操作，而其核心是实现在`fs/namei.c`的函数`path_openat`:

```c
static struct file *path_openat(struct nameidata *nd, const struct open_flags *op, unsigned flags)
{
	struct file *file;
	int error;

	file = alloc_empty_file(op->open_flag, current_cred());
	if (IS_ERR(file))
		return file;

	if (unlikely(file->f_flags & __O_TMPFILE)) {
		error = do_tmpfile(nd, flags, op, file);
	} else if (unlikely(file->f_flags & O_PATH)) {
		error = do_o_path(nd, flags, file);
	} else {
		const char *s = path_init(nd, flags);
		while (!(error = link_path_walk(s, nd)) &&
			(error = do_last(nd, file, op)) > 0) {
			nd->flags &= ~(LOOKUP_OPEN|LOOKUP_CREATE|LOOKUP_EXCL);
			s = trailing_symlink(nd);
		}
		terminate_walk(nd);
	}
	if (likely(!error)) {
		if (likely(file->f_mode & FMODE_OPENED))
			return file;
		WARN_ON(1);
		error = -EINVAL;
	}
	fput(file);
	if (error == -EOPENSTALE) {
		if (flags & LOOKUP_RCU)
			error = -ECHILD;
		else
			error = -ESTALE;
	}
	return ERR_PTR(error);
}
```

可见对于大部分情况而言，就是按照符号链接的路径找到最终的文件，然后用`do_last`打开文件。

## `name_to_handle_at`与`open_by_handle_at`

### 系统调用号

`name_to_handle_at`为303，`open_by_handle_at`为304。

### 函数原型

#### 内核接口

```c
asmlinkage long sys_name_to_handle_at(int dfd, const char __user *name, struct file_handle __user *handle, int __user *mnt_id, int flag);
asmlinkage long sys_open_by_handle_at(int mountdirfd, struct file_handle __user *handle, int flags);
```

#### glibc封装

```c
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
int name_to_handle_at(int dirfd, const char *pathname, struct file_handle *handle, int *mount_id, int flags);
int open_by_handle_at(int mount_fd, struct file_handle *handle, int flags);
```

### 简介

`name_to_handle_at`和`open_by_handle_at`将`openat`的功能解耦成两部分：

```plaintext
filename ----openat----> file descriptor
filename ----name_to_handle_at----> file handle + mount id ----open_by_handle_at----> file descriptor
```

`openat`将输入的文件路径转化为文件描述符输出，而`name_to_handle_at`将输入的文件路径转化为文件句柄和挂载ID输出，而`open_by_handle_at`将输入的文件句柄和挂载ID转化为文件描述符输出。

这样做看似多此一举，但是在构造无状态的文件服务器时很有用。假设我们的文件服务器是使用`openat`作为打开文件的方式：

1. 方案一
    * 步骤
	    1. 服务器告诉客户端文件路径
	    2. 客户端告诉服务器，想要修改的文件路径，和修改的内容
	    3. 服务器使用`openat`打开相应文件路径，然后对文件进行修改
    * 假设服务器告诉客户端我们的文件路径为`path/text.txt`，在客户端发送修改指令之前，又有另一个客户端将这个文件移动到了`path2/text.txt`，那么客户端的修改就失败了，因为`openat`无法打开相应的文件
2. 方案二
    * 步骤
	    1. 服务器用`openat`打开文件路径，告诉客户端文件描述符
	    2. 客户端告诉服务器，想要修改的文件描述符，和修改的内容
	    3. 服务器对相应的文件描述符作出相应的修改
    * 由于文件描述符在文件移动后依然有效，所以这么做确实可以避免方案一的问题。但是，我们的服务器就成了一个有状态的服务器。因为程序一旦挂掉，那么所有文件描述符都会失效

因此，用`openat`并不能完美解决无状态文件服务器的问题。但是，我们用`name_to_handle_at`和`open_by_handle_at`的方案为：

1. 服务器用`name_to_handle_at`打开文件路径，告诉客户端文件句柄和挂载ID
2. 客户端告诉服务器，想要修改的文件句柄和挂载ID，和修改的内容
3. 服务器使用`open_by_handle_at`打开相应的文件，获得文件描述符，进行进一步的修改

这一方案和方案二看起来很类似，但实际上有一点不同：文件句柄和挂载ID是由操作系统维护的，而不需要服务器维护。也就是说，只要是在同一操作系统中，所有的进程通过文件句柄和挂载ID打开的文件一定相同。

`name_to_handle_at`的具体行为，由`dirfd`, `pathname`和`flags`共同控制：

* 如果`pathname`是绝对路径，则返回对应的文件句柄和挂载ID，忽略`dirfd`
* 如果`pathname`是相对路径，则返回该路径相对于`dirfd`（若值为`AT_FDCWD`则是当前目录）对应的文件的文件句柄和挂载ID
* 如果`pathname`解析出来是符号链接，且`flags`包含标志位`AT_SYMLINK_FOLLOW`，则继续找该符号链接引用的真实文件，并返回真实文件的文件句柄和挂载ID
* 如果`pathname`为空字符串，且`flags`包含控制位`AT_EMPTY_PATH`，则返回对应于文件描述符`dirfd`的文件句柄和挂载ID（此时`dirfd`可以为任意文件类型的描述符，不一定是目录的文件描述符）

`open_by_handle_at`的`flags`则和`openat`的`flags`含义相同，为打开文件的方式。

### 用法

`struct file_handle`的定义为

```c
struct file_handle {
	unsigned int  handle_bytes;
	int           handle_type;
	unsigned char f_handle[0];
};
```

其中`f_handle`字段是变长数组。当我们使用`name_to_handle_at`时，首先需要将`handle`的`handle_bytes`字段置0，传入后，`name_to_handle_at`将返回-1，同时`errno`会被置为`EOVERFLOW`，同时`handle`的`handle_bytes`字段会被置为其需要的大小，然后我们再根据这个大小分配相应的空间给`handle`，再次传入即可：

```c
char filename[] = "./text.txt";
struct file_handle tmp_handle;
tmp_handle.handle_bytes = 0;
if (name_to_handle_at(filename, AT_FDCWD, &tmp_handle, NULL, 0) != -1 || errno != EOVERFLOW) {
	exit(-1); // Unexpected behavior
}
struct file_handle *handle = (struct file_handle *)malloc(tmp_handle.handle_bytes);
handle->handle_bytes = tmp_handle.handle_bytes;
int mount_id;
name_to_handle_at(filename, AT_FDCWD, handle, &mount_id, 0);
```

而`open_by_handle_at`则相对比较简单：

```c
int fd = open_by_handle_at(mount_id, handle, O_RDONLY);
```

### 实现

`name_to_handle_at`和`open_by_handle_at`的实现均位于`fs/fhandle.c`中。`name_to_handle_at`的核心为位于`fs/exportfs/expfs.c`中的`exportfs_encode_inode_fh`:

```c
int exportfs_encode_inode_fh(struct inode *inode, struct fid *fid, int *max_len, struct inode *parent)
{
	const struct export_operations *nop = inode->i_sb->s_export_op;

	if (nop && nop->encode_fh)
		return nop->encode_fh(inode, fid->raw, max_len, parent);

	return export_encode_fh(inode, fid, max_len, parent);
}
```

`open_by_handle_at`的核心为位于`fs/exportfs/expfs.c`中的`exportfs_decode_fh`:

```c
struct dentry *exportfs_decode_fh(struct vfsmount *mnt, struct fid *fid, int fh_len, int fileid_type, int (*acceptable)(void *, struct dentry *), void *context)
{
	const struct export_operations *nop = mnt->mnt_sb->s_export_op;
	// ...
	result = nop->fh_to_dentry(mnt->mnt_sb, fid, fh_len, fileid_type);
	// ...
}
```

由此可见，其关键在于`export_operations`这个结构体。通过位于内核源码`Documentation/filesystems/nfs/exporting.rst`的文档我们可以知道：

> encode_fh (optional)
>
>    Takes a dentry and creates a filehandle fragment which can later be used to find or create a dentry for the same object. The default implementation creates a filehandle fragment that encodes a 32bit inode and generation number for the inode encoded, and if necessary the same information for the parent.
>
> fh_to_dentry (mandatory)
>
>    Given a filehandle fragment, this should find the implied object and create a dentry for it (possibly with d_obtain_alias).

用于产生文件句柄的`encode_fh`函数指针，其默认实现是和inode直接相关的，所以确实可以看作是由操作系统来维护这个文件句柄的。

## `open_tree`

### 系统调用号

428

### 函数原型

#### 内核接口

```c
asmlinkage long sys_open_tree(int dfd, const char __user *path, unsigned flags);
```

#### glibc封装

无glibc封装，需要手动调用`syscall`。

### 简介

该系统调用的介绍在网络上较少，可以参考[lkml.org](https://lkml.org/lkml/2018/9/21/884)。

我们可以将该系统调用看作包含`O_PATH`标志位，用`open`或`openat`打开。也就是说，只在文件系统中标记该位置，而不打开相应的内容，产生的文件描述符只能供少部分在文件层次进行的操作使用。

当`flags`包含标志位`OPEN_TREE_CLONE`时，情况会稍微复杂一些。此时，`open_tree`会产生一个游离的(detached)挂载树，该挂载树对应的就是`dfd`和`path`决定的路径上的子树。如果还包含`AT_RECURSIVE`标志位，则整个子树都将被复制，否则就只复制目标挂载点内的部分。`OPEN_TREE_CLONE`标志位可以看作`mount --bind`，而`OPEN_TREE_CLONE | AT_RECURSIVE`标志位可以看作`mount --rbind`。

### 实现

位于`fs/namespace.c`文件中，其核心语句为

```c
file = dentry_open(&path, O_PATH, current_cred());
```

也就是通过`O_PATH`标志位打开相应的文件。
