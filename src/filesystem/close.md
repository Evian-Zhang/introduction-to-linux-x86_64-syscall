# `close`系统调用

## 系统调用号

3

## 函数签名

### 内核接口

```c
asmlinkage long sys_close(unsigned int fd);
```

### glibc封装

```c
#include <unistd.h>
int close(int fd);
```

## 简介

`close`系统调用的作用是关闭一个文件描述符，使得其不再指向任何文件，同时该文件描述符也可供后续`open`等操作复用。

`close`系统调用在我们日常的文件操作中随处可见，功能也相对比较简单。但是，有几点需要注意：

首先，如果我们在`open`状态下删除（`unlink`）了某个文件，那对这个文件描述符的`close`操作的行为，则需要判断该文件被多少个文件描述符所引用。假设`close`的文件描述符是最后一个引用该文件的描述符，则`close`操作之后，该文件将被真正意义上的删除。

第二，`close`并不能确保我们在之前对文件写入的数据一定会写入到硬盘中。正如我们在[`write`](./write-pwrite-writev-pwritev-pwritev2.md)中提到的，如果要确保在`close`之前数据写入到硬盘，就使用`fsync`。

第三，`close`如果失败，则返回-1，并且设置相应的`errno`。`close`可能失败的原因比较重要，主要包括：

* `EBADF`: `fd`不是一个有效的，处于打开状态的文件描述符
* `EINTR`: `close`操作被信号中断
* `EIO`: IO失败

虽然`close`失败后的行为和别的系统调用类似，但比较特殊的是，一般情况下，这种失败只能起到一种“告知”作用，我们不能再次使用`close`。这是因为在`close`的实现中，真正实施关闭文件的操作在整个过程的很前面，一般在文件真正关闭之前是不会产生错误的。因此，尽管`close`出错，但**有可能**这个文件描述符已经被关闭了。此时，再次调用`close`，如果该文件描述符没有被再次使用，则由于已经被关闭，所以会返回`EBADF`错误；否则，如果在再次调用`close`之前，该文件描述符被别的线程复用了，那就会不小心关闭了别的线程的有意义的文件描述符。

## 实现

`close`的实现位于`fs/open.c`中：

```c
SYSCALL_DEFINE1(close, unsigned int, fd)
{
	int retval = __close_fd(current->files, fd);

	/* can't restart close syscall because file table entry was cleared */
	if (unlikely(retval == -ERESTARTSYS ||
		     retval == -ERESTARTNOINTR ||
		     retval == -ERESTARTNOHAND ||
		     retval == -ERESTART_RESTARTBLOCK))
		retval = -EINTR;

	return retval;
}
```

可见其核心为位于`fs/file.c`的`__close_fd`函数:

```c
int __close_fd(struct files_struct *files, unsigned fd)
{
	struct file *file;
	struct fdtable *fdt;

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	if (fd >= fdt->max_fds)
		goto out_unlock;
	file = fdt->fd[fd];
	if (!file)
		goto out_unlock;
	rcu_assign_pointer(fdt->fd[fd], NULL);
	__put_unused_fd(files, fd);
	spin_unlock(&files->file_lock);
	return filp_close(file, files);

out_unlock:
	spin_unlock(&files->file_lock);
	return -EBADF;
}
```

从第13行可以看到，最先进行的实际操作是在该进程的文件描述符列表删除该描述符，然后在第16行使用位于`fs/open.c`的`filp_close`函数对文件描述符对应的文件做扫尾工作：

```c
int filp_close(struct file *filp, fl_owner_t id)
{
	int retval = 0;

	if (!file_count(filp)) {
		printk(KERN_ERR "VFS: Close: file count is 0\n");
		return 0;
	}

	if (filp->f_op->flush)
		retval = filp->f_op->flush(filp, id);

	if (likely(!(filp->f_mode & FMODE_PATH))) {
		dnotify_flush(filp, id);
		locks_remove_posix(filp, id);
	}
	fput(filp);
	return retval;
}
```

其核心也就是第11行的`filp->f_op->flush`，也就是如果文件系统有相应的`flush`操作，则对文件进行`flush`。在`fs/ext4/file.c`的512行就可以看到，我们在Linux中常用的EXT4系统，并没有定义相应的操作。也就是说，这里实际上也没做什么事。

从这个实现中我们可以验证两件事：

* 在`close`整个过程的早期，文件描述符就已经失效了，不再表示一个有效的打开状态的文件描述符，且在其完成之前基本上不会产生失败。
* 如果我们`flush`的操作失败了，是没有机会再次进行`flush`的，因为文件描述符先前已经失效了。
