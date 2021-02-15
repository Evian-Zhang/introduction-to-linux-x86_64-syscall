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

我们知道，绝大多数文件相关的系统调用都是直接操作文件句柄（file descriptor），而`open`和`openat`这两个系统调用是一种创建文件句柄的方式。`open`系统调用将打开路径为`filename`的文件，而`openat`则将打开相对句柄为`dfd`的目录，路径为`filename`的文件。

详细来说，`open`和`openat`的行为是

* `filename`是绝对路径
  * `open`打开位于`filename`的文件
  * `openat`打开位于`filename`的文件，忽略`dfd`
* `filename`是相对路径
  * `open`打开相对于当前目录，路径为`filename`的文件
  * `openat`打开相对于`dfd`对应的目录，路径为`filename`的文件；若`dfd`是定义在`fcntl.h`中的宏`AT_FDCWD`，则打开相对于当前目录，路径为`filename`的文件。

接着，是“怎么打开”的问题。`open`和`openat`的参数`flags`, `mode`控制了打开文件的行为（`mode`详情请见`creat`系统调用。TODO：增加超链接）。

#### flags

用于打开文件的标志均定义在`fcntl.h`头文件中。

##### 文件访问模式标志（file access mode flag）

* `O_RDONLY`<br/>以只读方式打开。创建的文件句柄不可写。
* `O_WDONLY`<br/>以只写方式打开。创建的文件句柄不可读。
* `O_RDWD`<br/>以读写方式打开。创建的文件句柄既可读也可写。
* `O_EXEC`<br/>以只执行方式打开。创建的文件句柄只可以被执行。只能用于非目录路径。
* `O_SEARCH`<br/>以只搜索方式打开。创建的文件句柄值可以被用于搜索。只能用于目录路径。

POSIX标准要求在打开文件时，必须且只能使用上述标志位中的一个。而glibc的封装则要求在打开文件时，必须且只能使用前三个标志位（只读、只写、读写）中的一个。

##### 文件创建标志（file creation flag）

文件创建标志控制的是`open`和`openat`在打开文件时的行为。部分比较常见的标志位有：

* `O_CLOEXEC`
  * 文件句柄将在调用`exec`函数族时关闭。
  * 我们知道，当一个Linux进程使用`fork`创建子进程后，父进程原有的文件句柄也会复制给子进程。而常见的模式是在`fork`之后使用`exec`函数族替换当前进程空间。此时，由于替换前的所有变量都不会被继承，所以文件句柄将丢失，而丢失之后就无法关闭相应的文件句柄，造成泄露。如[以下代码](https://github.com/Evian-Zhang/introduction-to-linux-x86_64-syscall/tree/master/codes/open-cloexec)：
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
    其中`./child`在启动30秒后会自动退出。<br/>
    在启动这个程序之后，我们使用`ps -aux | grep child`找到`child`对应的进程ID，然后使用
      ```shell
      readlink /proc/xxx/fd/yyy
      ```
    查看，其中`xxx`为进程ID，`yyy`是`fd`中的任意一个文件。我们调查`fd`中的所有文件，一定能发现一个文件句柄对应`text.txt`。也就是说，在执行`execve`之后，子进程始终保持着`text.txt`的句柄，且没有任何方法关闭它。
  * 解决这个问题的方法一般有两种：
    * 在`fork`之后，`execve`之前使用`close`关闭所有文件句柄。但是如果该进程在此之前创建了许多文件句柄，在这里就很容易漏掉，也不易于维护。
    * 在使用`open`创建文件句柄时，加入`O_CLOEXEC`标志位：
      ```c
      int fd = open("./text.txt", O_RDONLY | O_CLOEXEC);
      ```
      通过这种方法，在子进程使用`execve`时，文件句柄会自动关闭。
* `O_CREAT`
  * 当`filename`路径不存在时，创建相应的文件。
  * 使用此标志时，`mode`参数将作为创建的文件的文件模式标志位。详情请见`creat`系统调用。TODO: 增加超链接
* `O_EXCL`
  * 该标志位一般会与`O_CREAT`搭配使用。
  * 如果`filename`路径存在相应的文件（包括软链接），则`open`会失败。
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
  * 这种方式在异步IO模式中较少使用，主要由于这种基于中断的信号处理机制比系统调用的耗费更大，并且无法处理多个文件句柄同时完成IO操作。参考[What's the difference between async and nonblocking in unix socket?](https://stackoverflow.com/a/6260132/10005095)。
  * 对正常文件的句柄无效，对套接字等文件句柄有效。
* `O_NONBLOCK`
  * 后续的IO操作立即返回，而不是等IO操作完成后返回。
  * 对正常文件的句柄无效，对套接字等文件句柄有效。
* `O_SYNC`与`O_DSYNC`
  * 使用`O_SYNC`时，每一次`write`操作结束前，都会将文件内容和元信息写入相应的硬件。
  * 使用`O_DSYNC`时，每一次`write`操作结束前，都会将文件内容写入相应的硬件（不保证元信息）。
  * 这两种方法可以看作是在每一次`write`操作后使用`fsync`。
* `O_PATH`
  * 仅以文件句柄层次打开相应的文件。详情见下方`open_tree`系统调用的描述。

#### 其他注意点

此外，还有一些需要注意的。

在新的Linux内核（版本不低于2.26）中，glibc的封装`open`底层调用的是`openat`系统调用而不是`open`系统调用（`dfd`为`AT_FDCWD`）。

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

由其实现可知，无论路径是否一样，`flag`是否相同，`open`总会使用新的文件句柄。也就是说：

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

可见对于大部分情况而言，就是按照软链接的路径找到最终的文件，然后用`do_last`打开文件。
