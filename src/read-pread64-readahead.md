# `read`, `pread64`, `readahead`系统调用

## `read`与`pread64`

### 系统调用号

`read`的系统调用号为0, `pread64`的系统调用号为17。

### 函数签名

```c
asmlinkage long sys_read(unsigned int fd, char __user *buf, size_t count);
asmlinkage long sys_pread64(unsigned int fd, char __user *buf, size_t count, loff_t pos);
```

glibc封装后为

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

`read`和`pread64`的实现均在`fs/read_write.c`。