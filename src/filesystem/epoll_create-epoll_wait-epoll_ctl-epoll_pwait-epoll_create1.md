# `epoll_create`, `epoll_wait`, `epoll_ctl`, `epoll_pwait`, `epoll_create1`系统调用

## 系统调用号

* `epoll_create`: 213
* `epoll_wait`: 232
* `epoll_ctl`: 233
* `epoll_pwait`: 281
* `epoll_create1`: 291

## 函数原型

### 内核接口

```c
asmlinkage long sys_epoll_create(int size);
asmlinkage long sys_epoll_wait(int epfd, struct epoll_event __user *events, int maxevents, int timeout);
asmlinkage long sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event __user *event);
asmlinkage long sys_epoll_pwait(int epfd, struct epoll_event __user *events, int maxevents, int timeout, const sigset_t __user *sigmask, size_t sigsetsize);
asmlinkage long sys_epoll_create1(int flags);
```

### glibc封装

```c
#include <sys/epoll.h>
int epoll_create(int size);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask);
int epoll_create1(int flags);
```

### 简介

与[`select`和`poll`](./poll-select-pselect6-ppoll.md)一样，epoll机制也是为了实现IO多路复用。其使用方法更先进，内部实现也更高效。

我们可以理解为，Linux内核为了实现epoll机制，在内核空间维护了一个数据结构，称为epoll实例。其包含两个集合，一个是由用户感兴趣的文件描述符与相应的事件组成，另一个是由触发了相应事件的文件描述符与相应的事件组成。

我们的整体步骤是

1. 创建一个epoll实例
2. 对于epoll实例，将我们感兴趣的文件描述符与相应的事件添加到集合中
3. 从触发事件的集合中提取相应的文件描述符

### 使用

#### `epoll_create`与`epoll_create1`

其函数签名为

```c
int epoll_create(int size);
int epoll_create1(int flags);
```

这两个系统调用就是在内核空间创建一个epoll实例，返回该实例的文件描述符。

对于`epoll_create`，`size`参数是被忽略的，但其必须大于0。

`epoll_create1`是`epoll_create`的加强版。如果`flags`为0，则其行为与`epoll_create`一致。此外，`flags`还可以加入`EPOLL_CLOEXEC`标志位，和`open`中的`O_CLOEXEC`标志位功能一致，具体请看[相应的描述](./open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)。

由`epoll_create`和`epoll_create1`创建的文件描述符，也就是epoll实例对应的文件描述符也应在程序结束前使用`close`关闭。但我们应当注意，正如[在`open`中描述的](./open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)，如果使用了`dup`或者`fork`等会复制文件描述符的操作，我们将会有多个文件描述符指向Linux内核空间中的epoll实例。只有所有的指向该epoll实例的文件描述符都被关闭，其内核空间中的资源才会被释放。

#### `epoll_ctl`

其函数签名为

```c
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

其可以看作epoll机制中较为核心的系统调用。

总的来说，该系统调用接受的四个参数的意义为：

* `epfd`

    epoll实例的文件描述符

* `op`

    希望进行的操作

* `fd`

    感兴趣的文件描述符

* `events`

    对应于该文件描述符，感兴趣的事件

首先，`epfd`的意义很简单，就是我们调用`epoll_create`或`epoll_create1`返回的epoll实例的文件描述符。

`op`是我们希望进行的操作，包括：

* `EPOLL_CTL_ADD`

    向epoll实例的用户感兴趣的集合中增添元素，文件描述符由`fd`给出，感兴趣的事件由`event`给出

* `EPOLL_CTL_MOD`

    修改epoll实例的用户感兴趣的集合中的元素。希望修改的元素的文件描述符由`fd`给出，修改后的事件由`event`给出

* `EPOLL_CTL_DEL`

    删除epoll实例的用户感兴趣的集合中的元素。希望删除的元素的文件描述符由`fd`给出，`event`变量将被忽略，可以为`NULL`

而`fd`和`events`就是这个系统调用的核心参数。

`fd`可以**看作**在<b><code>epoll_ctl</code>阶段</b>，在epoll实例中，用于区分用户感兴趣的集合中的不同元素的方法。这是因为，我们的增加、修改、删除操作，都是基于`fd`来选择相应的元素的。

这里强调在`epoll_ctl`阶段，是因为实际上并不是以`fd`为键在集合中区分不同元素的。Linux内核采用的方法是将`fd`和其对应的文件描述一起，作为键来区分不同元素。我们考虑以下情况：

我们通过`dup`、`fork`等复制文件描述符的操作，创造了`fd1`和`fd2`这两个文件描述符，但是其都指向同一个文件描述。如果我们将`fd1`和`fd2`都加入epoll实例的用户感兴趣的集合，同时其对应的用户感兴趣的事件是不同的。然后，我们使用`close`关闭`fd1`。但由于文件描述没有被释放，在我们使用`epoll_wait`获取触发了的事件时，仍然会有`fd1`对应的事件报告出来。

因此，只有指向同一文件描述的所有文件描述符都被关闭，在epoll实例的用户感兴趣的集合中才会删除所有相应的元素。所以，我们在使用`close`关闭某个被加入epoll实例的文件描述符之前，记得要使用`EPOLL_CTL_DEL`操作先删除相应的元素。
