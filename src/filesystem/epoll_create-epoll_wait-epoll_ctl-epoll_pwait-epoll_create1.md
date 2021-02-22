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

## 简介

与[`select`和`poll`](./poll-select-pselect6-ppoll.md)一样，epoll机制也是为了实现IO多路复用。其使用方法更先进，内部实现也更高效。

我们可以理解为，Linux内核为了实现epoll机制，在内核空间维护了一个数据结构，称为epoll实例。其包含两个集合，一个是由用户感兴趣的文件描述符与相应的事件组成，另一个是由触发了相应事件的文件描述符与相应的事件组成。

我们的整体步骤是

1. 创建一个epoll实例
2. 对于epoll实例，将我们感兴趣的文件描述符与相应的事件添加到集合中
3. 从触发事件的集合中提取相应的文件描述符

## 使用

### `epoll_create`与`epoll_create1`

其函数签名为

```c
int epoll_create(int size);
int epoll_create1(int flags);
```

这两个系统调用就是在内核空间创建一个epoll实例，返回该实例的文件描述符。

对于`epoll_create`，`size`参数是被忽略的，但其必须大于0。

`epoll_create1`是`epoll_create`的加强版。如果`flags`为0，则其行为与`epoll_create`一致。此外，`flags`还可以加入`EPOLL_CLOEXEC`标志位，和`open`中的`O_CLOEXEC`标志位功能一致，具体请看[相应的描述](./open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)。

由`epoll_create`和`epoll_create1`创建的文件描述符，也就是epoll实例对应的文件描述符也应在程序结束前使用`close`关闭。但我们应当注意，正如[在`open`中描述的](./open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)，如果使用了`dup`或者`fork`等会复制文件描述符的操作，我们将会有多个文件描述符指向Linux内核空间中的epoll实例。只有所有的指向该epoll实例的文件描述符都被关闭，其内核空间中的资源才会被释放。

### `epoll_ctl`

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

这里有一点需要我们考虑。我们考虑以下情况：

我们通过`dup`、`fork`等复制文件描述符的操作，创造了`fd1`和`fd2`这两个文件描述符，但是其都指向同一个文件描述。如果我们将`fd1`和`fd2`都加入epoll实例的用户感兴趣的集合，同时其对应的用户感兴趣的事件是不同的。然后，我们使用`close`关闭`fd1`。但由于文件描述没有被释放，在我们使用`epoll_wait`获取触发了的事件时，仍然会有`fd1`对应的事件报告出来。

因此，只有指向同一文件描述的所有文件描述符都被关闭，在epoll实例的用户感兴趣的集合中才会删除所有相应的元素。所以，我们在使用`close`关闭某个被加入epoll实例的文件描述符之前，记得要使用`EPOLL_CTL_DEL`操作先删除相应的元素。

`event`参数的类型是`struct epoll_event`的指针。结合我们之后将到的`epoll_wait`，这个参数的作用是标记相应的事件。当我们把这个参数传递给`epoll_ctl`时，这个参数表明我们关心的事件。随后我们使用`epoll_wait`同样会获得这个类型的实例，其表明触发的事件。

`struct epoll_event`的定义为

```c
struct epoll_event {
    uint32_t     events;      /* Epoll events */
    epoll_data_t data;        /* User data variable */
};
```

首先来讲讲`data`字段。`epoll_data_t`的定义为

```c
typedef union epoll_data {
    void        *ptr;
    int          fd;
    uint32_t     u32;
    uint64_t     u64;
} epoll_data_t;
```

这个字段对epoll实例来说并不会起到实际的用途。当我们把`data`作为一个字段，放在某个用户感兴趣的`epoll_event`事件中，传入`epoll_ctl`函数，那么内核就会记录这个`data`。当相应的事件触发之后，用户使用`epoll_wait`等获得相应的事件，此时`data`会不经修改地出现在返回的事件中。

这个`data`常见的用途是，由于`epoll_wait`等方法获得事件时，无法直接获得该事件对应的文件描述符，所以我们在使用`epoll_ctl`时，将文件描述符作为`data`即可。随后在`epoll_wait`获得的事件中，取其`data`字段，获得相应的`fd`。

而`events`字段是epoll的核心。`events`字段是一个位掩码，其主要包含以下几种标志位：

* `EPOLLET`

    epoll处理的事件有两种模式：边沿触发（edge-triggered）与水平触发（level-triggered）。

    考虑以下情形：

    1. 我们向`epoll`实例注册一个文件描述符`rfd`，其代表某个管道的读端。我们关心其是否已经可读
    2. 从写端往管道里写了2KB数据
    3. 我们使用`epoll_wait`获得触发了的事件，其中包括我们事先注册的`rfd`
    4. 我们从`rfd`读取了1KB数据
    5. 我们再次使用`epoll_wait`

    如果是边沿触发模式，那么epoll只会在第3步的`epoll_wait`中给出`rfd`被触发的事件，第5步则不会给出相应的事件；如果是水平触发，那么epoll在第3步和第5步都会给出相应的事件。

    也就是说，只有在相应的文件描述符**状态发生变化**，从别的状态变成我们感兴趣的状态时，“边沿触发”才会给出我们相应的事件；只要相应的文件描述符**处于感兴趣的状态**时，“水平触发”就会给出我们相应的事件。

    因此，当我们使用边沿触发模式时，我们的`read`或`write`操作不能只使用一次，因为之后相关的事件就不会被触发，也就不能读取或写入完整的数据了。我们应当在循环中使用`read`或`write`，直到其返回错误`EAGAIN`（详见[open](./open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)）。同时由于`EAGAIN`错误只有在使用`O_NONBLOCK`标志位打开文件时才会出现，所以我们在使用边沿触发时要注意两点：

    * 使用非阻塞的文件描述符
    * `read`和`write`要一直读取或写入到返回`EAGAIN`错误

    如果我们的`events`包含标志位`EPOLLET`，则该事件是边沿触发模式；如果不包含该标志位，则该事件是水平触发模式。

    该标志位只可包含在传递给`epoll_ctl`的`events`中，不会出现在`epoll_wait`等返回的`events`中。

* `EPOLLIN`

    文件描述符已经可以被读取

* `EPOLLOUT`

    文件描述符已经可以被写入

* `EPOLLRDHUP`

    流套接字的对端关闭连接

* `EPOLLPRI`

    `select`中的其他条件，`poll`中的`POLLPRI`

* `EPOLLERR`

    出现错误。

    该标志位可能出现在`epoll_wait`等返回的`events`中，epoll默认关注这样的状态，因此并没有必要包含在传递给`epoll_ctl`的`events`中

* `EPOLLHUP`

    相关的文件描述符处于挂起状态。

    该标志位可能出现在`epoll_wait`等返回的`events`中，epoll默认关注这样的状态，因此并没有必要包含在传递给`epoll_ctl`的`events`中

* `EPOLLONESHOT`

    如果包含该标志位，该事件被触发，被`epoll_wait`等返回，那么该事件对应的文件描述符将不再会有别的事件被epoll实例关注。也就是说，如果该文件描述符别的事件出现了，epoll实例并不会返回相应的结果。

    如果要再次接受相应的事件，就应在`epoll_ctl`中使用`EPOLL_CTL_MOD`，给该事件新的事件掩码。

    在一个多线程程序中，如果我们在一个循环中调用epoll，每次获得一个触发的事件，就开启一个新的线程去处理，那么有可能某个状态没有改变，但是导致某个事件被多次触发，从而使得我们有多个线程去处理同一个文件描述符的状态。因此我们可以使用`EPOLLONESHOT`标志位来避免这种事。

    该标志位只可包含在传递给`epoll_ctl`的`events`中，不会出现在`epoll_wait`等返回的`events`中。

### `epoll_wait`与`epoll_pwait`

这两个系统调用的函数签名为

```c
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask);
```

`epoll_wait`与`epoll_pwait`就是从epoll实例的准备好的集合中，获取相应的事件。其参数包括：

* `epfd`为epoll实例的文件描述符。
* `events`为一个数组，其元素类型为`struct epoll_event`，长度为`maxevents`。
* `timeout`为超时参数。

对于`events`参数，epoll实例会从准备好的集合中，选取至多`maxevents`个事件放入该数组中。

对于`timeout`参数，其精度为毫秒，如果其为-1，则`epoll_wait`将无限等待；如果其为0，则`epoll_wait`将立即返回。

`epoll_wait`的返回值为准备好的文件描述符的个数。

与`pselect`和`ppoll`类似，`epoll_pwait`就是加上了信号掩码的`epoll_wait`。

因此，如果使用epoll来实现我们在[`select`与`poll`](./poll-select-pselect6-ppoll.md)中提出的方案三，其方法为：

```c
void process_fds(int *fds, int nfd) {
    int epfd = epoll_create1(0);
    for (int i = 0; i < nfd; i++) {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = fds[i];
        epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i], &ev);
    }
    struct epoll_event *events = (struct epoll_event *)malloc(nfd * sizeof(struct epoll_event));
    while (1) {
        int ready_nfd = epoll_wait(epfd, events, nfd, -1);
        if (ready_nfd <= 0) {
            break;
        }
        for (int i = 0; i < ready_nfd; i++) {
            if (events[i].events & EPOLLIN) {
                char buf[64];
                read(events[i].data.fd, buf, 64);
                process_read_content(buf);
            }
        }
    }
}
```

## 实现

epoll的实现位于Linux内核的`fs/eventpoll.c`文件中。

在epoll的实现中，有两个结构体最为关键：`struct eventpoll`与`struct epitem`。

`struct eventpoll`就是内核中的epoll实例的结构体，而`struct epitem`就是一个文件描述符与它相关的事件组成的结构体，也就是epoll实例的两个集合的元素。

它们的**部分**字段如下：

`struct eventpoll`:

```c
struct eventpoll {
	/* Wait queue used by sys_epoll_wait() */
	wait_queue_head_t wq;

	/* Wait queue used by file->poll() */
	wait_queue_head_t poll_wait;

	/* List of ready file descriptors */
	struct list_head rdllist;

	/* RB tree root used to store monitored fd structs */
	struct rb_root_cached rbr;

	/*
	 * This is a single linked list that chains all the "struct epitem" that
	 * happened while transferring ready events to userspace w/out
	 * holding ->lock.
	 */
	struct epitem *ovflist;

	/* wakeup_source used when ep_scan_ready_list is running */
	struct wakeup_source *ws;

	struct list_head visited_list_link;
};
```

`struct epitem`:

```c
struct epitem {
	union {
		/* RB tree node links this structure to the eventpoll RB tree */
		struct rb_node rbn;
		/* Used to free the struct epitem */
		struct rcu_head rcu;
	};

	/* List header used to link this structure to the eventpoll ready list */
	struct list_head rdllink;

	/*
	 * Works together "struct eventpoll"->ovflist in keeping the
	 * single linked chain of items.
	 */
	struct epitem *next;

	/* The file descriptor information this item refers to */
	struct epoll_filefd ffd;

	/* Number of active wait queue attached to poll operations */
	int nwait;

	/* List containing poll wait queues */
	struct list_head pwqlist;

	/* The "container" of this item */
	struct eventpoll *ep;

	/* List header used to link this item to the "struct file" items list */
	struct list_head fllink;

	/* wakeup_source used when EPOLLWAKEUP is set */
	struct wakeup_source __rcu *ws;

	/* The structure that describe the interested events and the source fd */
	struct epoll_event event;
};
```

首先是`struct rb_root_cached rbr`这个字段。这就是epoll实例中用于存储用户感兴趣的事件的结构。它是一个红黑树，其包含的元素可以看作我们的`struct epitem`（其字段`rbn`就是表示在这棵红黑树的节点）。当我们使用`epoll_create`创建一个epoll实例时，这棵红黑树被初始化。当我们使用`epoll_ctl`去操作感兴趣的集合时，我们实际上就是增添、修改、删除这棵红黑树的元素。

这里值得注意的是，我们之前提到，在`epoll_ctl`阶段，可以把文件描述符**看作**集合的键，在我们操作这个集合的时候，通过这个键来区分不同的元素。但实际并不是这样。

以这棵红黑树的插入为例，其实现为`ep_rbtree_insert`:

```c
static void ep_rbtree_insert(struct eventpoll *ep, struct epitem *epi)
{
	int kcmp;
	struct rb_node **p = &ep->rbr.rb_root.rb_node, *parent = NULL;
	struct epitem *epic;
	bool leftmost = true;

	while (*p) {
		parent = *p;
		epic = rb_entry(parent, struct epitem, rbn);
		kcmp = ep_cmp_ffd(&epi->ffd, &epic->ffd);
		if (kcmp > 0) {
			p = &parent->rb_right;
			leftmost = false;
		} else
			p = &parent->rb_left;
	}
	rb_link_node(&epi->rbn, parent, p);
	rb_insert_color_cached(&epi->rbn, &ep->rbr, leftmost);
}
```

可以看到在11行，通过调用`ep_cmp_ffd`来判断是否两个元素相同。首先我们来看看`struct epitem`的`ffd`字段，其类型为`struct epoll_filefd`:

```c
struct epoll_filefd {
	struct file *file;
	int fd;
} __packed;
```

而`ep_cmp_ffd`的实现为

```c
static inline int ep_cmp_ffd(struct epoll_filefd *p1, struct epoll_filefd *p2)
{
	return (p1->file > p2->file ? +1:
	        (p1->file < p2->file ? -1 : p1->fd - p2->fd));
}
```

因此我们可以看到，内核是同时使用文件描述与文件描述符作为这棵红黑树的键的。

如果我们想通过`epoll_ctl`增加一个我们感兴趣的元素，我们做的核心实际上是增加了一个回调函数。首先我们需要知道，在`include/linux/poll.h`中，我们对某个文件的`poll`操作，其最终是这样的情形：

```c
/* 
 * structures and helpers for f_op->poll implementations
 */
typedef void (*poll_queue_proc)(struct file *, wait_queue_head_t *, struct poll_table_struct *);

/*
 * Do not touch the structure directly, use the access functions
 * poll_does_not_wait() and poll_requested_events() instead.
 */
typedef struct poll_table_struct {
	poll_queue_proc _qproc;
	__poll_t _key;
} poll_table;

static inline __poll_t vfs_poll(struct file *file, struct poll_table_struct *pt)
{
	if (unlikely(!file->f_op->poll))
		return DEFAULT_POLLMASK;
	return file->f_op->poll(file, pt);
}
```

我们使用`vfs_poll`之后，会把`file`和`pt`传入其对应的实现中。而`pt`是`struct poll_table_struct`的指针，其中，`_key`字段是一个掩码，表明哪些事件是用户关注的；`poll_queue_proc _qproc`是一个函数指针。当出现了`_key`中的事件时，会自动触发这个回调函数。

当我们使用`epoll_ctl`去创建新的红黑树节点时，有一步为

```c
/* Initialize the poll table using the queue callback */
epq.epi = epi;
init_poll_funcptr(&epq.pt, ep_ptable_queue_proc);

/*
 * Attach the item to the poll hooks and get current event bits.
 * We can safely use the file* here because its usage count has
 * been increased by the caller of this function. Note that after
 * this operation completes, the poll callback can start hitting
 * the new item.
 */
revents = ep_item_poll(epi, &epq.pt, 1);
```

这就是我们设置相应回调函数的地方。`epq`的类型是`struct ep_pqueue`，其定义为

```c
struct ep_pqueue {
	poll_table pt;
	struct epitem *epi;
};
```

也就是说把`poll_table`封装了一层。我们通过`init_poll_funcptr`设置了`epq`的`poll_table`，然后通过`ep_item_poll`把这个`poll_table`传入了最终的`vfs_poll`函数中。

在epoll中，`poll_table`的`_key`字段，也就是用户感兴趣的事件是全部事件，epoll会从触发的事件中筛选出用户感兴趣的事件。回调函数则是`ep_ptable_queue_proc`，其设置了回调函数`ep_poll_callback`。

TODO：增加更多的描述。参考资料：

* [the-implementation-of-epoll-1](https://idndx.com/the-implementation-of-epoll-1/)
* [the-implementation-of-epoll-2](https://idndx.com/the-implementation-of-epoll-2/)
* [the-implementation-of-epoll-3](https://idndx.com/the-implementation-of-epoll-3/)
* [the-implementation-of-epoll-4](https://idndx.com/the-implementation-of-epoll-4/)
