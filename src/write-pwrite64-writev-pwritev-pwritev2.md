# `write`, `pwrite64`, `writev`, `pwritev`, `pwritev2`系统调用

## `write`与`pwrite64`

### 系统调用号

`write`的系统调用号为1，`pwrite64`的系统调用号为18。

### 函数签名

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

`write`和`pwrite`是最基础的对文件写入的系统调用。`write`会将`buf`中`count`个字节写入句柄为`fd`的文件中，而`pread`则会将`buf`中`count`个字节写入句柄为`fd`的文件从`offset`开始的位置中。如果写入成功，这两个系统调用都将返回写入的字节数。因此，这两个系统调用主要的区别就在于写入的位置，其它功能均类似。

注意点：

首先，是文件偏移的问题：

* 写入前位置<br/>显然，`pwrite`是从文件偏移为`offset`的位置开始写入，但是`write`的问题则比较特殊。一般来说，`write`开始写入时的文件偏移就是当前的文件偏移，但是，当文件句柄是通过`open`系统调用创建，且创建时使用了`O_APPEND`标志位的话，每次`write`开始写入前，都会默认将文件偏移移到文件末尾。
* 写入后位置<br/>同`read`和`pread`类似，`write`在成功写入n个字节后，会将文件偏移更新n个字节；但`pwrite`则不会更新文件偏移，因此和`pread`一起常用于多线程的代码中。

我们可以通过一个简单的程序检测这个性质

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

```
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

* 首先，我们使用`O_APPEND`标志位创建文件句柄`fd_with_append`。
  1. 使用`read`读入4字节。在读入前文件偏移为0，读入成功4字节，文件偏移为4，且读入的字符串为"1234"。
  2. 使用`write`写入7字节长度字符串"payload"。在写入前，文件偏移被移至文件末尾6，因此成功写入7字节后，文件偏移为13。
  3. 使用`read`读入4字节。在读入前，文件偏移为13，处于文件末尾，无内容读入，所以读入字节为0，读入后文件偏移依然为13。
  4. 最终，`text.txt`的内容为"123456payload"
* 接着，我们不使用`O_APPEND`标志位创建文件句柄`fd_without_append`。
  1. 使用`read`读入4字节。