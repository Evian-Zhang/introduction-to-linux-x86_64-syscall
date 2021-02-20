# `poll`, `select`, `pselect6`, `ppoll`系统调用

## `select`与`poll`

### 系统调用号

`poll`为7，`select`为23。

### 函数原型

#### 内核接口

```c
asmlinkage long sys_poll(struct pollfd __user *ufds, unsigned int nfds, int timeout);
asmlinkage long sys_select(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
```

#### glibc封装

`poll`

```c
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

`select`

```c
#include <sys/select.h>
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
```

### 简介

`select`与`poll`都是为了实现IO多路复用的功能。

一般来说，对硬盘上的文件的读取都不会阻塞。但是，对管道、套接字、伪终端等文件的读取，是可能产生阻塞的。举个例子来说，如果我们读取`stdin`：

```c
int fd = STDIN_FILENO; // stdin
char buf[64];
read(fd, buf, 64); // blocks here
process_read_content(buf);
```

那么，在执行`read`时，如果我们一直不向终端输入，那么这里会始终阻塞着，程序永远不会执行到之后的`process_read_context(buf)`。在这种情况下，这种行为是符合逻辑的，因为我们之后的语句是依赖读入的内容`buf`的。所以除非我们收到了`buf`的内容，否则就不应该执行之后的指令。

但是，如果有多个文件需要读入，就产生了问题。假设我们有一个`nfd`个元素的文件描述符数组`fds`，我们需要对他们读入，并彼此独立地分别处理每个读入的内容。

* 方案一
    ```c
    void process_fds(int *fds, int nfd) {
        for (int i = 0; i < nfd; i++) {
            char buf[64];
            read(fds[i], buf, 64);
            process_read_content(buf);
        }
    }
    ```
    这个方案能完成我们的需求，但是效率实在是太低了。由于是按顺序依次处理读入的内容，如果`fds[0]`始终没有输入，但是`fds[1]`早就有了输入。我们明明可以先处理`fds[1]`的输入的，但是由于进程阻塞在了`fds[0]`的`read`操作中，我们的时间就这样被白白浪费了。
* 方案二

    既然每个文件描述符处理读入是互相独立的，我们就可以创建`nfd`个线程，每个线程中处理其读入。

    这种方案确实可以解决我们方案一中的问题，但是线程的创建、线程之间的切换是非常耗费时间的。

为了更高效地解决这个问题，我们可以增加一个新的操作——判断某个文件描述符是否可以读入。我们可以遍历文件描述符，判断是否有已经可以读入的，如果有的话就直接处理，如果没有的话就再次遍历。这样几乎没有耗费的时间。

我们甚至可以想出更高效的方案，在主线程中查询是否有可以读取的文件描述符，然后把可以读取的文件描述符给别的线程执行。

`select`就为我们提供了一个类似的解决方案。我们给`select`传入需要检测IO状态（可以读入、可以写入等）的文件描述符集合，`select`立即返回，告诉我们哪些文件描述符的IO已经准备就绪。

`poll`的功能和`select`类似，但解决了一些`select`的缺点。具体请见下面的用法一节。

### 用法

#### `select`

`select`的函数签名为

```c
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
```

`readfds`, `writefds`和`exceptfds`是文件描述符集合，分别用于：

* `readfds`

    已经准备好供读取的文件描述符集合，即`read`操作不会阻塞。

* `writefds`

    已经准备好供写入的文件描述符集合，即`write`操作不会阻塞。

* `exceptfds`

    其余条件的文件描述符集合。包括：
    * TCP套接字有带外数据
    * 处于包模式下的伪终端的主端检测到从端的状态变化
    * `cgroup.events`文件被修改

如果对相应的状态变化不感兴趣，在对应的参数中传递`NULL`即可。

我们可以用以下几个函数操作`fd_set`类型的变量：

```c
void FD_CLR(int fd, fd_set *set);
int  FD_ISSET(int fd, fd_set *set);
void FD_SET(int fd, fd_set *set);
void FD_ZERO(fd_set *set);
```

* `FD_ZERO`

    将`set`清空

* `FD_SET`

    将`fd`放入`set`中

* `FD_CLR`

    将`fd`从`set`中移除

* `FD_ISSET`

    判断`fd`是否处于`set`中

`nfds`为`readfds`, `writefds`, `exceptfds`中，数值最大的文件描述符加1。如`readfds`包含文件描述符4, 6, 7，`writefds`包含文件描述符5，`exceptfds`为空，则`nfds`为8。

`timeout`为超时参数，其结构为

```c
struct timeval {
    time_t      tv_sec;         /* seconds */
    suseconds_t tv_usec;        /* microseconds */
};
```

如果`timeout`指针为`NULL`，则`select`将一直等待，直到有一个文件描述符准备好。如果`tv_sec`和`tv_usec`均为0，则`select`将立即返回。否则，`select`如果等待达到`timeout`的时间，还没有任何文件描述符准备好，就返回。

当函数返回之后，会有如下变化：

* 返回值为`readfds`, `writefds`, `exceptfds`中准备好的文件描述符的总数
* `readfds`, `writefds`, `exceptfds`中会只保留已经处于准备好状态的文件描述符。我们可以通过`FD_ISSET`去查看哪些文件描述符准备好。（正因如此，如果我们在一个循环中使用`select`，那在每次使用之前，需要复制一遍各集合，或用`FD_CLR`清空后重新添加）
* `select`可能会更新`timeout`参数。

综上，如果我们要按第三个方案来实现我们的功能，其写法为

```c
void process_fds(int *fds, int nfd) {
    fd_set rset;
    FD_ZERO(&rset);
    int maxfd = -1;
    for (int i = 0; i < nfd; i++) {
        FD_SET(fds[i], &rset);
        if (fds[i] > maxfd) {
            maxfd = fds[i];
        }
    }
    while (1) {
        fd_set tmp_rset = rset;
        if (select(maxfd + 1, &tmp_rset, NULL, NULL, NULL) <= 0) {
            break;
        }
        for (int i = 0; i < nfd; i++) {
            if (FD_ISSET(fds[i], &tmp_rset)) {
                FD_CLR(fds[i], &rset);
                char buf[64];
                read(fds[i], buf, 64);
                process_read_content(buf);
            }
        }
    }
}
```

此外，值得注意的是，glibc的封装要求我们每个集合包含的文件描述符不能超过`FD_SETSIZE`，也就是1024个。

## `pselect6`与`ppoll`

### 系统调用号

`pselect6`为270，`ppoll`为271。

### 函数签名

#### 内核接口

```c
asmlinkage long sys_pselect6(int, fd_set __user *, fd_set __user *, fd_set __user *, struct __kernel_timespec __user *, void __user *);
asmlinkage long sys_ppoll(struct pollfd __user *, unsigned int, struct __kernel_timespec __user *, const sigset_t __user *, size_t);
```

#### glibc封装

`pselect6`

```c
#include <sys/select.h>
int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout,  const sigset_t *sigmask);
```

`ppoll`

```c
#define _GNU_SOURCE
#include <signal.h>
#include <poll.h>
int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigmask);
```
