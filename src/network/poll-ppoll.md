# `poll`, `ppoll`系统调用

## 系统调用号

`poll`为7，`ppoll`为271。

## 函数原型

### 内核接口

```c
asmlinkage long sys_poll(struct pollfd __user *ufds, unsigned int nfds, int timeout);
asmlinkage long sys_ppoll(struct pollfd __user *, unsigned int, struct __kernel_timespec __user *, const sigset_t __user *, size_t);
```

### glibc封装

`poll`

```c
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

`ppoll`

```c
#define _GNU_SOURCE
#include <signal.h>
#include <poll.h>
int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigmask);
```

## 简介
