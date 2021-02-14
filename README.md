# Linux x86_64系统调用简介

如果要对Linux上的恶意软件进行细致地分析，或者想了解Linux内核，又或是想对Linux内核做进一步的定制，那么了解Linux的全部系统调用就是一个很好的帮助。在分析Linux上的恶意软件时，如果对Linux的系统调用不了解，那么往往会忽视某些重要的关键细节。因此，在充分了解Linux的系统调用之后，就能做到有的放矢，更好地达到我们的需求。

本仓库将详细记录每个Linux x86_64系统调用的功能、用法与实现细节。

## 环境

所有实现细节均基于[v5.4版本的Linux内核](https://github.com/torvalds/linux/tree/v5.4)，系统调用列表位于`arch/x86/entry/syscalls/syscall_64.tbl`。功能、用法基于其相应的manual page，可在[man7.org](https://www.man7.org/linux/man-pages/)中查看。涉及到验证代码的，则会在Ubuntu 20.04中进行验证，glibc版本为2.31。


## 系统调用对照表

每个系统调用名都超链接到了其在本仓库中对应的文章。

|名称|系统调用号|头文件|内核实现|
|-|-|-|-|
|[`read`](src/read-pread64-readv-preadv-preadv2.md)|0|`unistd.h`|`fs/read_write.c`|
|[`write`](src/write-pwrite64-writev-pwritev-pwritev2.md)|1|`unistd.h`|`fs/read_write.c`|
|[`pread64`](src/read-pread64-readv-preadv-preadv2.md)|17|`unistd.h`|`fs/read_write.c`|
|[`pwrite64`](src/write-pwrite64-writev-pwritev-pwritev2.md)|18|`unistd.h`|`fs/read_write.c`|
|[`readv`](src/read-pread64-readv-preadv-preadv2.md)|19|`sys/uio.h`|`fs/read_write.c`|
|[`writev`](src/write-pwrite64-writev-pwritev-pwritev2.md)|20|`sys/uio.h`|`fs/read_write.c`|
|[`preadv`](src/read-pread64-readv-preadv-preadv2.md)|295|`sys/uio.h`|`fs/read_write.c`|
|[`pwritev`](src/write-pwrite64-writev-pwritev-pwritev2.md)|296|`sys/uio.h`|`fs/read_write.c`|
|[`preadv2`](src/read-pread64-readv-preadv-preadv2.md)|327|`sys/uio.h`|`fs/read_write.c`|
|[`pwritev2`](src/write-pwrite64-writev-pwritev-pwritev2.md)|328|`sys/uio.h`|`fs/read_write.c`|
