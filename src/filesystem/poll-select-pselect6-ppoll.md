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
