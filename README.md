# Linux x86_64系统调用简介

如果要对Linux上的恶意软件进行细致地分析，或者想了解Linux内核，又或是想对Linux内核做进一步的定制，那么了解Linux的全部系统调用就是一个很好的帮助。在分析Linux上的恶意软件时，如果对Linux的系统调用不了解，那么往往会忽视某些重要的关键细节。因此，在充分了解Linux的系统调用之后，就能做到有的放矢，更好地达到我们的需求。

本仓库将详细记录每个Linux x86_64系统调用的功能、用法与实现细节。

## 环境

所有实现细节均基于[v5.4版本的Linux内核](https://github.com/torvalds/linux/tree/v5.4)，glibc-2.31版本的glibc。所记录的系统调用列表位于Linux内核源码的`arch/x86/entry/syscalls/syscall_64.tbl`。功能、用法基于其相应的manual page，可在[man7.org](https://www.man7.org/linux/man-pages/)中查看。涉及到验证代码的，则会在Ubuntu 20.04中进行验证。

如果仅想阅读本仓库的文章以及相应的测试代码，可以使用

```shell
git clone git@github.com:Evian-Zhang/introduction-to-linux-x86_64-syscall.git
```

如果想同时把相应版本的Linux内核源码与glibc源码一并下载，可以使用

```shell
git clone --recurse-submodules git@github.com:Evian-Zhang/introduction-to-linux-x86_64-syscall.git
```

对于在国内的网友，可以考虑使用清华大学开源软件镜像站：

```shell
git clone git@github.com:Evian-Zhang/introduction-to-linux-x86_64-syscall.git
cd introduction-to-linux-x86_64-syscall
git config submodule.linux.url https://mirrors.tuna.tsinghua.edu.cn/git/linux.git
git config submodule.glibc.url https://mirrors.tuna.tsinghua.edu.cn/git/glibc.git
git pull --recurse-submodules
```

## 系统调用对照表

每个系统调用名都超链接到了其在本仓库中对应的文章。

|名称|系统调用号|头文件|内核实现|
|-|-|-|-|
|[`read`](src/filesystem/read-pread64-readv-preadv-preadv2.md)|0|`unistd.h`|`fs/read_write.c`|
|[`write`](src/filesystem/write-pwrite64-writev-pwritev-pwritev2.md)|1|`unistd.h`|`fs/read_write.c`|
|[`open`](src/filesystem/open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)|2|`fcntl.h`|`fs/open.c`|
|[`close`](src/filesystem/close.md)|3|`unistd.h`|`fs/open.c`|
|[`stat`](src/filesystem/stat-fstat-lstat-newfstatat-statx.md)|4|`sys/stat.h`|`fs/stat.c`|
|[`fstat`](src/filesystem/stat-fstat-lstat-newfstatat-statx.md)|5|`sys/stat.h`|`fs/stat.c`|
|[`lstat`](src/filesystem/stat-fstat-lstat-newfstatat-statx.md)|6|`sys/stat.h`|`fs/stat.c`|
|[`poll`](src/filesystem/poll-select-pselect6-ppoll.md)|7|`poll.h`|`fs/select.c`|
|[`lseek`](src/filesystem/lseek.md)|8|`unistd.h`|`fs/read_write.c`|
|[`mmap`](src/memory_management/mmap-munmap-mremap-msync-remap_file_pages.md)|9|`sys/mman.h`|`arch/x86/kernel/sys_x86_64.c`|
|[`munmap`](src/memory_management/mmap-munmap-mremap-msync-remap_file_pages.md)|11|`sys/mman.h`|`mm/mmap.c`|
|[`pread64`](src/filesystem/read-pread64-readv-preadv-preadv2.md)|17|`unistd.h`|`fs/read_write.c`|
|[`pwrite64`](src/filesystem/write-pwrite64-writev-pwritev-pwritev2.md)|18|`unistd.h`|`fs/read_write.c`|
|[`readv`](src/filesystem/read-pread64-readv-preadv-preadv2.md)|19|`sys/uio.h`|`fs/read_write.c`|
|[`writev`](src/filesystem/write-pwrite64-writev-pwritev-pwritev2.md)|20|`sys/uio.h`|`fs/read_write.c`|
|[`select`](src/filesystem/poll-select-pselect6-ppoll.md)|23|`sys/select.h`|`fs/select.c`|
|[`mremap`](src/memory_management/mmap-munmap-mremap-msync-remap_file_pages.md)|25|`sys/mman.h`|`mm/mremap.c`|
|[`msync`](src/memory_management/mmap-munmap-mremap-msync-remap_file_pages.md)|26|`sys/mman.h`|`mm/msync.c`|
|[`epoll_create`](src/filesystem/epoll_create-epoll_wait-epoll_ctl-epoll_pwait-epoll_create1.md)|213|`sys/epoll.h`|`fs/eventpoll.c`|
|[`remap_file_pages`](src/memory_management/mmap-munmap-mremap-msync-remap_file_pages.md)|216|`sys/mman.h`|`mm/mmap.c`|
|[`epoll_ctl`](src/filesystem/epoll_create-epoll_wait-epoll_ctl-epoll_pwait-epoll_create1.md)|232|`sys/epoll.h`|`fs/eventpoll.c`|
|[`epoll_wait`](src/filesystem/epoll_create-epoll_wait-epoll_ctl-epoll_pwait-epoll_create1.md)|233|`sys/epoll.h`|`fs/eventpoll.c`|
|[`openat`](src/filesystem/open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)|257|`fcntl.h`|`fs/open.c`|
|[`newfstatat`](src/filesystem/stat-fstat-lstat-newfstatat-statx.md)|262|`sys/stat.h`|`fs/stat.c`|
|[`pselect6`](src/filesystem/poll-select-pselect6-ppoll.md)|270|`sys/select.h`|`fs/select.c`|
|[`ppoll`](src/filesystem/poll-select-pselect6-ppoll.md)|271|`poll.h`|`fs/select.c`|
|[`epoll_pwait`](src/filesystem/epoll_create-epoll_wait-epoll_ctl-epoll_pwait-epoll_create1.md)|281|`sys/epoll.h`|`fs/eventpoll.c`|
|[`eventfd`](src/filesystem/eventfd-eventfd2.md)|284|`sys/eventfd.h`|`fs/eventfd.c`|
|[`eventfd2`](src/filesystem/eventfd-eventfd2.md)|290|`sys/eventfd.h`|`fs/eventfd.c`|
|[`epoll_create1`](src/filesystem/epoll_create-epoll_wait-epoll_ctl-epoll_pwait-epoll_create1.md)|291|`sys/epoll.h`|`fs/eventpoll.c`|
|[`preadv`](src/filesystem/read-pread64-readv-preadv-preadv2.md)|295|`sys/uio.h`|`fs/read_write.c`|
|[`pwritev`](src/filesystem/write-pwrite64-writev-pwritev-pwritev2.md)|296|`sys/uio.h`|`fs/read_write.c`|
|[`name_to_handle_at`](src/filesystem/open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)|303|`fcntl.h`|`fs/fhandle.c`|
|[`open_by_handle_at`](src/filesystem/open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)|304|`fcntl.h`|`fs/fhandle.c`|
|[`preadv2`](src/filesystem/read-pread64-readv-preadv-preadv2.md)|327|`sys/uio.h`|`fs/read_write.c`|
|[`pwritev2`](src/filesystem/write-pwrite64-writev-pwritev-pwritev2.md)|328|`sys/uio.h`|`fs/read_write.c`|
|[`statx`](src/filesystem/stat-fstat-lstat-newfstatat-statx.md)|332|`linux/stat.h`|`fs/stat.c`|
|[`open_tree`](src/filesystem/open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)|428|无|`fs/namespace.c`|

#### License

<sup>
本仓库遵循<a href="https://creativecommons.org/licenses/by/4.0/">CC-BY-4.0版权协议</a>。
</sup>

<br>

<sub>
作为<a href="https://copyleft.org/">copyleft</a>的支持者之一，我由衷地欢迎大家积极热情地参与到开源社区中。Happy coding!
</sub>
