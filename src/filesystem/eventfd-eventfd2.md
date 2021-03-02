# `eventfd`, `eventfd2`系统调用

## 系统调用号

* `eventfd`: 284
* `eventfd2`: 290

## 函数原型

### 内核接口

```c
asmlinkage long sys_eventfd(unsigned int count);
asmlinkage long sys_eventfd2(unsigned int count, int flags);
```

### glibc封装

```c
#include <sys/eventfd.h>

int eventfd(unsigned int initval, int flags);
```

## 简介

自内核 2.6.22 起，Linux 通过 `eventfd()` 系统调用额外提供了一种非标准的同步机制。这个系统调用创建了一个 `eventfd` 对象，该对象拥有一个相关的由内核维护的 8 字节无符号整数。通知机制就建立在这个无符号整数的数值变化上。

这个系统调用返回一个指向该 `eventfd` 的文件描述符。用户可以对这个文件描述符使用 `read` 或 `write` 系统调用，来操作由内核维护的数值。

此外，`eventfd` 可以和 `epoll` 等多路复用的系统调用一同使用：我们可以使用多路复用的系统调用测试对象值是否为非零，如果是非零的话就表示文件描述符可读。

在 linux 2.6.22 之后，`eventfd` 可用。在 linux 2.6.27 之后，`eventfd2` 可用。他们二者的区别是，`eventfd` 系统调用没有 `flags` 参数。而 glibc 从2.9开始，提供的 `eventfd` 底层则会调用 `eventfd2` 来进行实现（如果 `eventfd2` 被内核支持的话）。

因此如果内核版本不支持，您务必要将 `flags` 设置为0，除此之外这两个系统调用没有差别。

## 使用

### 函数签名

```c
int eventfd(unsigned int initval, int flags);
```

- `unsigned int initval`: 内核中维护的无符号整数的初始值，我们一般叫它 counter

- `int flags`: 

  - 2.6.26 及之前，这个flags还不支持，必须设置成0
  - flags 设置后，会影响对 `eventfd` 对象操作（如 `read` `write`）时的行为，有三种：
    - `EFD_CLOEXEC`
    - `EFD_NONBLOCK`
    - `EFD_SEMAPHORE`
- `return val int`: 返回一个指向该对象的文件描述符


### 配合 `read` 和 `write` 使用

- `read(2)`
  - `EFD_SEMAPHORE` 如果没有被设置，从 eventfd read，会得到 counter，并将它归0
  - `EFD_SEMAPHORE` 如果被设置，从 eventfd read，会得到值 1，并将 counter - 1
  - counter 为 0 时，对它进行 read
    - `EFD_NONBLOCK` 如果被设置，那么会以 `EAGAIN` 的错失败
     - 否则 read 会被阻塞，直到为非0。
- `write(2)`
  - 会把一个 8 字节的int值写入到 counter 里
  - 最大值是 2^64 - 1
  - 如果写入时会发生溢出，则write会被阻塞
    - 如果 EFD_NONBLOCK 被设置，那么以 EAGAIN 失败
  - 以不合法的值写入时，会以 EINVAL 失败
    - 比如 0xffffffffffffffff 不合法
    - 比如 写入的值 size 小于8字节

### 配合多路复用使用

`poll(2)`,  `select(2)`, `epoll(7)`

- 作为被监听的 fd 用于多路复用API
- 如果 counter 的值大于 0 ，那么 fd 的状态就是「可读的」
  - `select` 中 `readfds` 生效
  - `poll` 中 `POLLIN` 生效
- 如果能无阻塞地写入一个至少为 1 的值，那么 fd 的状态就是「可写的」
  - `select` 中 `writefds` 生效
  - `poll` 中 `POLLOUT` 生效

## 作用

- 所有通过 pipe(2) 来进行**通知（而非数据传输）**操作的，都可以用 `eventfd(2)` 来代替
  - 节省文件描述符资源：pipe需要两个文件描述符，eventfd只需要一个
  - 内存开销小：内核管理层面，后者开销比前者低
    - 后者只需要一个counter，8字节大小
    - 前者在内核和用户态之间会来回拷贝多次，还会分配额外的虚拟内存页
- 提供了内核台与用户态之间沟通的桥梁
  - 比如kernel AIO，内核在事件完成后可以通过eventfd，通知用户态结构
- `eventfd` 既可以监听传统的文件，也可以监听内核提供的类似`epoll`、`select`实例文件
- `eventfd` 是并发安全的

## 例子

使用效果：

```
$ ./a.out 1 2 4 7 14
Child writing 1 to efd
Child writing 2 to efd
Child writing 4 to efd
Child writing 7 to efd
Child writing 14 to efd
Child completed write loop
Parent about to read
Parent read 28 (0x1c) from efd
```

源代码：

```c
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>             /* Definition of uint64_t */

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)
// 这个例子展示了 eventfd 在进程间通知的作用
int
main(int argc, char *argv[])
{
    int efd, j;
    uint64_t u;
    ssize_t s;
   // 接受至少一个数字
   if (argc < 2) {
        fprintf(stderr, "Usage: %s <num>...\n", argv[0]);
        exit(EXIT_FAILURE);
    }
   // flags 为 0
   // 初始值为 0
   efd = eventfd(0, 0);
   if (efd == -1)
        handle_error("eventfd");
   // fork 出子进程
   switch (fork()) {
    case 0: // 子进程下执行
        for (j = 1; j < argc; j++) {
            // 子进程负责把传入的参数写到 counter 里
            printf("Child writing %s to efd\n", argv[j]);
            u = strtoull(argv[j], NULL, 0);
                    /* strtoull() allows various bases */
            s = write(efd, &u, sizeof(uint64_t));
            if (s != sizeof(uint64_t))
                handle_error("write");
        }
        printf("Child completed write loop\n");

       exit(EXIT_SUCCESS);

   default: // 父进程下
        sleep(2); // 父进程暂时阻塞

       printf("Parent about to read\n");
       	// 阻塞结束后，父进程应当读到子进程写入的值之和
        s = read(efd, &u, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            handle_error("read");
        printf("Parent read %llu (0x%llx) from efd\n",
                (unsigned long long) u, (unsigned long long) u);
        exit(EXIT_SUCCESS);

   case -1:
        handle_error("fork");
    }
}
```

你可以修改一下代码，体验一下它如何通过 `read` `write` 进行通知作用的。

## 实现

eventfd 的实现位于Linux内核的`fs/eventfd.c`文件中。其中 `struct eventfd_ctx` 即是我们上文所说的 `counter` 。

```c
struct eventfd_ctx {
	struct kref kref;
	wait_queue_head_t wqh;
	/*
	 * Every time that a write(2) is performed on an eventfd, the
	 * value of the __u64 being written is added to "count" and a
	 * wakeup is performed on "wqh". A read(2) will return the "count"
	 * value to userspace, and will reset "count" to zero. The kernel
	 * side eventfd_signal() also, adds to the "count" counter and
	 * issue a wakeup.
	 */
	__u64 count;
	unsigned int flags;
	int id;
};
```

可以看到，`eventfd` 的实现其实就是通过内核维护的一个等待队列来控制进程的唤醒和阻塞。

TODO：

- 详细的 `eventfd` 的分析
- `eventfd` 和多路复用系统调用的配合使用例子。

