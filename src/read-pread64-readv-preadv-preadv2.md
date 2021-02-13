# `read`, `pread64`, `readv`, `preadv`, `preadv2`系统调用

## `read`与`pread64`

### 系统调用号

`read`的系统调用号为0, `pread64`的系统调用号为17。

### 函数签名

#### 内核接口

```c
asmlinkage long sys_read(unsigned int fd, char __user *buf, size_t count);
asmlinkage long sys_pread64(unsigned int fd, char __user *buf, size_t count, loff_t pos);
```

#### glibc封装

```c
#include <unistd.h>
ssize_t read(int fd, void *buf, size_t count);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
```

### 简介

`read`和`pread`是最基础的对文件读取的系统调用。`read`会从句柄为`fd`的文件中读取`count`个字节存入`buf`中，而`pread`则是从句柄为`fd`的文件中，从`offset`位置开始，读取`count`个字节存入`buf`中。如果读取成功，这两个系统调用都将返回读取的字节数。因此，这两个系统调用主要的区别就在于读取的位置，其它功能均类似。

有几点需要注意：

首先，是从哪开始读。`pread64`没有问题，就是从`offset`的位置开始读。而对于`read`，如果它读取的句柄对应的文件支持seek，那么它是从文件句柄中存储的文件偏移（file offset）处继续读。假设我们的文件对应的二进制数据为

```
1F 2E 3D 4C 5B 6A
```

我们先是用`read`读取了3个字节的内容，此时文件因为之前没有被读过，因此文件偏移为0，`read`将读到`1F 2E 3D`。然后，文件偏移就被更新为3。那么，我们接下来如果用`read`读取1个字节的内容，读到的将会是`4C`。

因此，对于支持seek的文件来说，`read`是从文件偏移的位置继续读，`pread`是从`offset`的位置开始读。

我们知道，更改文件偏移有单独的系统调用`lseek`，因此，如果我们要从某个特定的位置读取数据，可以`lseek`+`read`，也可以`pread`。但是，系统调用实际上是一个复杂的耗时操作，所以`pread`就用一次系统调用解决了两个系统调用的问题。

第二，是读多少的问题。`read`和`pread`读取的字节数一定不大于`count`，但有可能小于`count`。假设说我们的二进制文件只有上述6个字节。那么，如果我们`read`了8个字节，后2个字节自然是无法被读取的。因此，只能读取到6个字节，`read`也将返回6。除此之外，还有很多可能会让`read`和`pread`读取的字节小于`count`。比如说，从一个终端读取（输入的字节小于其需求的字节），或者在读取时被某些信号中断。

此外，除了读的字节小于`count`之外，`read`和`pread`还有可能读取失败。此时的返回值将是-1。我们可以用`errno`查看其错误。文件句柄不可读或无效（`EBADF`），`buf`不可使用（`EFAULT`），文件句柄是目录而非文件（`EISDIR`）等等，这些都有可能直接造成读取的错误。

第三，是读完当`read`或`pread`读取结束后的工作。`read`会更新文件句柄中的文件偏移，它们读了多少字节，就向后移动多少字节。但是，值得注意的是，`pread`并不会更新文件偏移。`pread`不更新文件偏移这一点对于多线程的程序来说极其有用。我们知道，多条线程有可能共用同一个文件句柄，但文件偏移是存储在文件句柄中。如果我们在多线程中使用`read`，会导致文件偏移混乱；但是，如果我们使用`pread`，则会完满避免这个问题。

第四，是如何读。在Linux的哲学中，如何读并不是`read`和`pread`能决定的，而是由文件句柄本身决定的。文件句柄在创建的时候，就决定了它将被如何读取，比如说是否阻塞等等。

### 用例

```c
#include <unistd.h>
#include <fcntl.h>

int main() {
    int fd = open("./text.txt", O_RDONLY);
    if (fd < 0) {
        // open error
        exit(1);
    }
    char buf[64];
    ssize_t read_length = read(fd, buf, 64);
    if (read_length < 0) {
        // read error
        exit(1);
    }
    ssize_t pread_length = pread(fd, buf, 64, 233);
    if (pread_length < 0) {
        // pread error
        exit(1);
    }
    close(fd);
    return 0;
}
```

### 实现

`read`和`pread64`的实现均位于`fs/read_write.c`。这两个系统调用的核心是`__vfs_read`函数，其实现为

```c
ssize_t __vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	if (file->f_op->read)
		return file->f_op->read(file, buf, count, pos);
	else if (file->f_op->read_iter)
		return new_sync_read(file, buf, count, pos);
	else
		return -EINVAL;
}
```

我们可以看到，它实际上是调用了`file->f_op->read(file, buf, count, pos)`函数。也就是我们上面讲到的，`read`和`pread`怎么读，是由文件句柄决定的。而第二个条件分支的`new_sync_read`我们之后会提到。我们可以把这个操作看作是用C实现的C++的多态，`file->f_op->read`是一个函数指针，类似于一个虚函数。每一种文件类型都会定义自己的`read`的方法，而`__vfs_read`则是调用了这个虚函数的方法。

总的来说，完成了这个函数之后，就实现了文件读取功能。

`read`和`pread64`的完整实现也很类似。`read`的实现主要是在`ksys_read`函数中，其实现为

```c
ssize_t ksys_read(unsigned int fd, char __user *buf, size_t count)
{
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos, *ppos = file_ppos(f.file);
		if (ppos) {
			pos = *ppos;
			ppos = &pos;
		}
		ret = vfs_read(f.file, buf, count, ppos);
		if (ret >= 0 && ppos)
			f.file->f_pos = pos;
		fdput_pos(f);
	}
	return ret;
}
```

可以看到，`read`会确定当前的文件偏移（第7行），然后从当前的文件偏移开始读取；读取完毕后，更新文件偏移（第14行）。

而`pread64`的实现主要是在`ksys_pread64`函数中，其实现为

```c
ssize_t ksys_pread64(unsigned int fd, char __user *buf, size_t count, loff_t pos)
{
	struct fd f;
	ssize_t ret = -EBADF;

	if (pos < 0)
		return -EINVAL;

	f = fdget(fd);
	if (f.file) {
		ret = -ESPIPE;
		if (f.file->f_mode & FMODE_PREAD)
			ret = vfs_read(f.file, buf, count, &pos);
		fdput(f);
	}

	return ret;
}
```

可以看到，它与`read`的实现主要的区别在于，它不需要读取当前的文件偏移，而是直接从`pos`处开始；读取完毕后，它也不会更新当前的文件偏移。

## `readv`, `preadv`与`preadv2`

### 系统调用号

`readv`为19，`preadv`为295，`preadv2`为327。

### 函数签名

```c
asmlinkage long sys_readv(unsigned long fd, const struct iovec __user *vec, unsigned long vlen);
asmlinkage long sys_preadv(unsigned long fd, const struct iovec __user *vec, unsigned long vlen, unsigned long pos_l, unsigned long pos_h);
asmlinkage long sys_preadv2(unsigned long fd, const struct iovec __user *vec, unsigned long vlen, unsigned long pos_l, unsigned long pos_h, rwf_t flags);
```

glibc封装后为

```c
#include <sys/uio.h>
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset);
ssize_t preadv2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags);
```

### 简介

我们在上面提到，`pread`除了在多线程中发挥大作用之外，也可以将两次系统调用`lseek`+`read`化为一次系统调用。而这一节所讲的系统调用，则是更进一步。`read`和`pread`是将文件读取到一块连续内存中，那如果我们想要将文件读取到多块连续内存中（也就是说，有多块内存，内存内部连续，但内存之间不连续），就得多次使用这些系统调用，造成很大的开销。而`readv`, `preadv`, `preadv2`则是为了解决这样的问题。

首先，我们需要知道`iovec`的定义（位于`include/uapi/linux/uio.h`）：

```c
struct iovec
{
	void __user *iov_base;
	__kernel_size_t iov_len;
};
```

这实际上就是`read`的后两个参数，也就是内存中的目的地址，与需要读取的长度。

`readv`, `preadv`, `preadv2`的第二个参数`iov`是一个`iovec`结构体组成的数组，其元素个数由第三个参数`iovcnt`给出。这三个系统调用的作用就是“分散读”（scatter input），将一块连续的文件内容，按顺序读入多块连续区域中。

在`preadv`和`preadv2`中，我们可以看到，其系统调用接口含有两个参数`pos_l`与`pos_h`，但glibc封装后只有一个参数`offset`。这是因为，考虑到64位地址的问题，`pos_l`和`pos_h`分别包含了`offset`的低32位和高32位。

此外，还需要注意，这里的读取虽然说是“向量化”，但实际上，缓冲区是按数组顺序处理的，也就是说，只有在`iov[0]`被填满之后，才会去填充`iov[1]`。

同样类似`read`与`pread`，这三个系统调用也是返回读取的字节数，同样可能会小于`iov->iov_len`之和。

与`read`和`pread`不同的是，这三个系统调用是原子性的，它们读取的文件内容永远是连续的，也就是说不会因为文件偏移被别的线程改变而混乱。比如说，我们想将文件中的内容读入三块缓冲区中。如果我们是使用三次`read`，但是在第一次`read`结束之后，第二次`read`开始之前，另外一个线程对这个文件句柄的文件偏移进行了改变，那么接下来的两次`read`读出的数据与第一次`read`读出的数据是不连续的。但是，如果我们用`readv`，读出的数据一定是连续的。

而`preadv`与`preadv2`的区别，主要在于最后一个参数。它通过一些标志位来改变读取的行为。具体可以看其手册[preadv2](https://www.man7.org/linux/man-pages/man2/preadv2.2.html)。

关于文件偏移的更新，`readv`和`read`一样，在结束之后会更新文件偏移；`preadv`和`pread`一样，在结束之后不会更新文件偏移；对于`preadv2`来说，如果`offset`为-1，其会使用当前的文件偏移而不是前往指定的文件偏移，并且在结束后会更新文件偏移，但是如果其不为-1，则不会更新文件偏移。

### 用例

```c
#include <sys/uio.h>
#include <fcntl.h>

int main() {
    int fd = open("./text.txt", O_RDONLY);
    if (fd < 0) {
        // open error
        exit(1);
    }
    char buf1[64], buf2[32], buf3[128];
    struct iovec iovecs[3];
    iovec[0] = (struct iovec){ .iov_base = buf1, .iov_len = 64 };
    iovec[1] = (struct iovec){ .iov_base = buf2, .iov_len = 32 };
    iovec[2] = (struct iovec){ .iov_base = buf3, .iov_len = 128 };
    ssize_t readv_length = readv(fd, iovecs, 3);
    if (readv_length < 0) {
        // readv error
        exit(1);
    }
    close(fd);
    return 0;
}
```

### 实现

这三者实现的核心为`call_read_iter`函数，位于`include/linux/fs.h`文件中：

```c
static inline ssize_t call_read_iter(struct file *file, struct kiocb *kio, struct iov_iter *iter)
{
	return file->f_op->read_iter(kio, iter);
}
```

就像之前讲的一样，“怎么读”是由文件类型本身决定的，这里就是`file->f_op->read_iter`这个函数指针。

而其余结构则与`read`和`pread`的实现类似。

同时，可以指出，之前`read`和`pread`的实现中，第二个条件分支

```c
if (file->f_op->read_iter)
		return new_sync_read(file, buf, count, pos);
```

就是防止文件类型只实现了`read_iter`而没有实现`iter`，因此用长度为1的数组调用`file->f_op->read_iter`。