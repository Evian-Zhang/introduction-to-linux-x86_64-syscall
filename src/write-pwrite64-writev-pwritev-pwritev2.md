# `write`, `pwrite64`, `writev`, `pwritev`, `pwritev2`系统调用

## `write`与`pwrite64`

### 系统调用号

`write`的系统调用号为1，`pwrite64`的系统调用号为18。

### 函数原型

#### 内核接口

```c
asmlinkage long sys_write(unsigned int fd, const char __user *buf, size_t count);
asmlinkage long sys_pwrite64(unsigned int fd, const char __user *buf, size_t count, loff_t pos);
```

#### glibc封装

```c
#include <unistd.h>
ssize_t write(int fd, const void *buf, size_t count);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
```

### 简介

`write`和`pwrite`是最基础的对文件写入的系统调用。`write`会将`buf`中`count`个字节写入描述符为`fd`的文件中，而`pread`则会将`buf`中`count`个字节写入描述符为`fd`的文件从`offset`开始的位置中。如果写入成功，这两个系统调用都将返回写入的字节数。因此，这两个系统调用主要的区别就在于写入的位置，其它功能均类似。

注意点：

首先，是文件偏移的问题：

* 写入前位置

    显然，`pwrite`是从文件偏移为`offset`的位置开始写入，但是`write`的问题则比较特殊。一般来说，`write`开始写入时的文件偏移就是当前的文件偏移，但是，当文件描述符是通过`open`系统调用创建，且创建时使用了`O_APPEND`标志位的话，每次`write`开始写入前，都会默认将文件偏移移到文件末尾。

* 写入后位置

    同`read`和`pread`类似，`write`在成功写入n个字节后，会将文件偏移更新n个字节；但`pwrite`则不会更新文件偏移，因此和`pread`一起常用于多线程的代码中。

我们可以通过[一个简单的程序](https://github.com/Evian-Zhang/introduction-to-linux-x86_64-syscall/tree/master/codes/write-pwrite)检测这个性质

```c
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

void test_file_offset(int fd) {
    char read_buf[16];
    char write_buf[] = "payload";
    printf("File offset is %zd.\n", lseek(fd, 0, SEEK_CUR));
    ssize_t read_length = read(fd, read_buf, 4);
    read_buf[read_length] = '\0';
    printf("Read %zd bytes: %s.\n", read_length, read_buf);
    printf("File offset is %zd.\n", lseek(fd, 0, SEEK_CUR));
    printf("Write %zd bytes.\n", write(fd, write_buf, 7));
    printf("File offset is %zd.\n", lseek(fd, 0, SEEK_CUR));
    read_length = read(fd, read_buf, 4);
    read_buf[read_length] = '\0';
    printf("Read %zd bytes: %s.\n", read_length, read_buf);
    printf("File offset is %zd.\n", lseek(fd, 0, SEEK_CUR));
}

int main() {
    int fd_with_append = open("./text.txt", O_RDWR | O_APPEND);
    printf("File opened with O_APPEND:\n");
    test_file_offset(fd_with_append);
    close(fd_with_append);
    int fd_without_append = open("./text.txt", O_RDWR);
    printf("File opened without O_APPEND:\n");
    test_file_offset(fd_without_append);
    close(fd_without_append);
    return 0;
}
```

输出为

```plaintext
File opened with O_APPEND:
File offset is 0.
Read 4 bytes: 1234.
File offset is 4.
Write 7 bytes.
File offset is 13.
Read 0 bytes: .
File offset is 13.
File opened without O_APPEND:
File offset is 0.
Read 4 bytes: 1234.
File offset is 4.
Write 7 bytes.
File offset is 11.
Read 2 bytes: ad.
File offset is 13.
```

我们在同目录中有一个文本文件`text.txt`，它的内容为六字节长的字符串"123456"。

* 首先，我们使用`O_APPEND`标志位创建文件描述符`fd_with_append`。
  1. 使用`read`读入4字节。在读入前文件偏移为0，读入成功4字节，文件偏移为4，且读入的字符串为"1234"。
  2. 使用`write`写入7字节长度字符串"payload"。在写入前，文件偏移被移至文件末尾6，因此成功写入7字节后，文件偏移为13。
  3. 使用`read`读入4字节。在读入前，文件偏移为13，处于文件末尾，无内容读入，所以读入字节为0，读入后文件偏移依然为13。
  4. 最终，`text.txt`的内容为"123456payload"。
* 接着，我们不使用`O_APPEND`标志位创建文件描述符`fd_without_append`。
  1. 使用`read`读入4字节。在读入前文件偏移为0，读入成功4字节，文件偏移为4，且读入的字符串为"1234"。
  2. 使用`write`写入7字节长度字符串"payload"。写入前文件偏移为4，写入成功7字节，文件偏移为11。此时`text.txt`的内容为"1234payloadad"。
  3. 使用`read`读入4字节。在读入前，文件偏移为11，文件总长度为13字节，所以只能读入成功2字节，读入后文件偏移为13，读入的字符串为"ad"。
  4. 最终，`text.txt`的内容为"1234payload"。

根据我们这个例子，很好地解释了文件偏移与`read`, `write`的关系。此外，还有一些需要注意的：

第一，在不使用`O_APPEND`标志位创建文件的例子中，为什么写入后文件的内容为`1234payloadad`。在写入前，由于上一轮的修改，文件的内容为"123456payload"。此时文件偏移为4，接下来将从"56..."的位置开始写入。而`write`如果写入的位置之后还有数据，是直接覆盖的，因此覆盖了7个字节，就变成了"1234payloadad"。这在[POSIX标准](https://pubs.opengroup.org/onlinepubs/9699919799/)中有提及：

> After a `write()` to a regular file has successfully returned:
>
> * Any subsequent successful `write()` to the same byte position in the file shall overwrite that file data.

第二，和`read`类似，`write`和`pwrite`返回的成功写入的字节数，可能会小于传入的参数`count`。这可能是由多种原因引起，比如说此硬盘分区的容量已满，或者超过了当前文件系统允许的单个文件的最大体积，此时，只能尽可能多地写入字节。比如说此硬盘分区还有2K字节就满了，我们企图写入5K字节，那么只能写入成功2K字节，所以`write`将返回2K。

但是，尽管如此，`write`和`pwrite`并不能保证在成功返回后，数据一定已经被写入硬盘。在某些情况下，甚至写入的错误也并不一定立刻出现。因此当我们再一次调用文件修改的操作，如`write`, `fsync`, `close`等时，就有可能出现错误。我们可以通过在写入数据后调用`fsync`，或是在`open`创建文件时使用`O_SYNC`或`O_DSYNC`标志位来解决这一问题。

虽然不能保证数据一定写入硬盘，POSIX标准同样规定了一件事：

> After a `write()` to a regular file has successfully returned:
>
> * Any successful `read()` from each byte position in the file that was modified by that write shall return the data specified by the `write()` for that position until such byte positions are again modified.

也就是说，即使不保证写入硬盘，`read`读入的数据一定是`write`成功之后的数据。

### 实现

`write`和`pwrite`的实现均位于`fs/read_write.c`中，其核心为`__vfs_write`函数：

```c
static ssize_t __vfs_write(struct file *file, const char __user *p, size_t count, loff_t *pos)
{
	if (file->f_op->write)
		return file->f_op->write(file, p, count, pos);
	else if (file->f_op->write_iter)
		return new_sync_write(file, p, count, pos);
	else
		return -EINVAL;
}
```

和`read`与`pread`类似，`write`和`pwrite`将调用文件类型对应的`write`函数指针，如果不存在，则调用其用于`writev`, `pwritev`的`write_iter`函数指针。

TODO: 对于常用的EXT4文件系统，找到『当文件描述符创建时使用`O_APPEND`标志位时，`write`系统调用会从文件末尾开始写入』这个特性的实现。

## `writev`, `pwritev`与`pwritev2`

### 系统调用号

`writev`为20，`pwritev`为296，`pwritev2`为328。

### 函数原型

#### 内核接口

```c
asmlinkage long sys_writev(unsigned long fd, const struct iovec __user *vec, unsigned long vlen);
asmlinkage long sys_pwritev(unsigned long fd, const struct iovec __user *vec, unsigned long vlen, unsigned long pos_l, unsigned long pos_h);
asmlinkage long sys_pwritev2(unsigned long fd, const struct iovec __user *vec, unsigned long vlen, unsigned long pos_l, unsigned long pos_h, rwf_t flags);
```

#### glibc封装

```c
#include <sys/uio.h>
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);
ssize_t pwritev2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags);
```

### 简介

和`readv`, `preadv`, `preadv2`类似，这三个系统调用是为了解决一次性从多个连续内存向一个文件描述符写入的问题，这三个系统调用被称为“聚合写”（gather output）。

这三个系统调用的特性与`readv`, `preadv`, `preadv2`十分类似，这里不再赘述。
