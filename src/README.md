# Linux x86_64系统调用简介

如果要对Linux上的恶意软件进行细致地分析，或者想了解Linux内核，又或是想对Linux内核做进一步的定制，那么了解Linux的全部系统调用就是一个很好的帮助。在分析Linux上的恶意软件时，如果对Linux的系统调用不了解，那么往往会忽视某些重要的关键细节。因此，在充分了解Linux的系统调用之后，就能做到有的放矢，更好地达到我们的需求。

本仓库将详细记录每个Linux x86_64系统调用的功能、用法与实现细节。

## 环境

所有实现细节均基于[v5.4版本的Linux内核](https://github.com/torvalds/linux/tree/v5.4)，系统调用列表位于`arch/x86/entry/syscalls/syscall_64.tbl`。功能、用法基于其相应的manual page，可在[man7.org](https://www.man7.org/linux/man-pages/)中查看。涉及到验证代码的，则会在Ubuntu 20.04中进行验证。

## 背景知识介绍

在本仓库中，如无特殊说明，处理器指令集默认为x86_64指令集。

### 系统调用简介

在Linux中，内核提供一些操作的接口给用户态的程序使用，这就是系统调用。对于用户态的程序，其调用相应的接口的方式，是一条汇编指令`syscall`。

比如说，创建子进程的操作，Linux内核提供了`fork`这个系统调用作为接口。那么，如果用户态程序想调用这个内核提供的接口，其对应的汇编语句为（部分）

```assembly
movq $57, %rax
syscall
```

`syscall`这个指令会先查看此时RAX的值，然后找到系统调用号为那个值的系统调用，然后执行相应的系统调用。我们可以在系统调用列表中找到，`fork`这个系统调用的系统调用号是57。于是，我们把57放入`rax`寄存器中，然后使用了`syscall`指令。这就是让内核执行了`fork`。

### 调用约定

提到这个，就不得不说Linux x86_64的调用约定。我们知道，系统调用往往会有许多参数，比如说`open`这个打开文件的系统操作，我们可以在`include/linux/syscall.h`中找到其对应的C语言接口为

```c
asmlinkage long sys_open(const char __user *filename, int flags, umode_t mode);
```

它接受三个参数。那么，参数传递是按照什么规定呢？事实上，当涉及到系统调用时，调用约定与用户态程序一般的调用约定并不相同。在[System V Application Binary Interface AMD64 Architecture Processor Supplement](https://gitlab.com/x86-psABIs/x86-64-ABI)的A.2.1节中我们可以看到：

> 1. User-level applications use as integer registers for passing the sequence `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8` and `%r9`. The kernel interface uses `%rdi`, `%rsi`, `%rdx`, `%r10`, `%r8` and `%r9`.
> 2. A system-call is done via the `syscall` instruction. The kernel destroys registers `%rcx` and `%r11`.
> 3. The number of the syscall has to be passed in register `%rax`.
> 4. System-calls are limited to six arguments, no argument is passed directly on the stack.
> 5. Returning from the syscall, register `%rax` contains the result of the system-call. A value in the range between -4095 and -1 indicates an error, it is `-errno`.
> 6. Only values of class INTEGER or class MEMORY are passed to the kernel.

比较重要的是前五点。从这五点我们可以知道，如果要调用`open`系统调用，那么步骤是：

1. 将`pathname`放入`rdi`寄存器
2. 将`flags`放入`rsi`寄存器
3. 将`mode`放入`rdx`寄存器
4. 将`open`的系统调用号2放入`rax`寄存器
5. 执行`syscall`指令
6. 返回值位于`rax`寄存器

我们使用逆向工具查看汇编代码时，就是通过类似以上六步的方法，确定一个系统调用的相关信息的。

这个规范就称为内核接口的调用约定，可以从第一点就显著地看到，这个调用约定与用户态的程序是不同的。也就是说，如果我们用编译器直接编译

```c
long sys_open(const char *pathname, int flags, mode_t mode);
```

那么，编译出来的可执行程序会认为，这个函数是用户态函数，其传参仍然是按 `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`的顺序，与内核接口不符。因此，gcc提供了一个标签`asmlinkage`来标记这个函数是内核接口的调用约定：

```c
asmlinkage long sys_open(const char *pathname, int flags, mode_t mode);
```

当函数前面有这个标签时，编译器编译出的可执行程序就会认为是按内核接口的调用约定对这个函数进行调用的。详情可以看[FAQ/asmlinkage](https://kernelnewbies.org/FAQ/asmlinkage)。

### glibc封装

当然，我们平时写的代码中，99%不会直接用到上述的系统调用方法。当我们真的去写一个C程序时：

```c
// syscall-wrapper-test.c
#include <unistd.h>

int main() {
    fork();
    return 0;
}
```

然后我们将其编译为汇编代码

```shell
gcc syscall-wrapper-test.c -S -o syscall-wrapper-test.S
```

只能看到这个指令：

```assembly
callq fork
```

然后在整个汇编文件中都不会找到`fork`这个函数的实现。甚至我们如果将其编译为可执行程序

```shell
gcc syscall-wrapper-test.c -o syscall-wrapper-test
```

然后用逆向工具去反汇编，也会发现整个可执行程序中也不会有`fork`的实现，同时也不会找到任何对57这个系统调用号进行`syscall`的代码。

这是因为，我们在Linux上编写的程序，通常都会链接到glibc的动态链接库。我们用

```shell
ldd syscall-wrapper-list
```

查看其链接的动态链接库，就会看到

```
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
```

而glibc则提供了许多系统调用的封装。这使我们在编写程序的时候，并不需要直接和内核进行交互，而是借用glibc这层封装，更加安全、稳定地使用。关于glibc对系统调用的封装，详情请见官方文档[SyscallWrappers](https://sourceware.org/glibc/wiki/SyscallWrappers)。

当然，如果真的想在可执行程序中直接对内核进行系统调用，可以把glibc静态链接：

```shell
gcc syscall-wrapper-test.c -static -o syscall-wrapper-test
```

然后在`syscall-wrapper-test`这个可执行程序中就可以看到直接的`syscall`了。

对glibc的动态链接和静态链接各有利弊。对于恶意软件编写者来说，他们往往倾向于静态链接恶意软件。这是因为，分析者可以轻松地写一个动态链接库，将其使用的glibc中的API hook住，改变其行为，使它达不成目的。而如果静态链接，那么分析者只有通过修改内核等比较麻烦的方案才能改变其行为。而静态链接的坏处则在于，如果简单地使用`-static`选项进行静态链接，就是把整个库都链接进最终的可执行程序中。这会导致库中许多没有被用到的函数的代码也在可执行程序中，使可执行程序的体积增大。解决方案可以参考gcc的官方文档[Compilation-options](https://gcc.gnu.org/onlinedocs/gnat_ugn/Compilation-options.html)。

### 内核接口

我们之前提到，在Linux内核中，可以在`include/linux/syscall.h`文件中找到系统调用函数的声明（会加上`sys_`前缀）。而其实现则是使用`SYSCALL_DEFINEn`这个宏。比如说，我们在`fs/open.c`中可以看到

```c
SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
{
	/* ... */
}
```

这代表：

* 内核提供了一个接口，接受三个参数
* 这个接口叫`open`
* 第一个参数的类型是`const char __user *`，参数名为`filename`
* 第二个参数的类型是`int`，参数名是`flags`
* 第三个参数的类型是`umode_t`，参数名是`mode`

## 系统调用对照表

每个系统调用名都超链接到了其在本仓库中对应的文章。

| 名称            | 系统调用号 | 头文件     | 内核实现          |
| --------------- | ---------- | ---------- | ----------------- |
| [`read`](./read-pread-readahead.md)      | 0          | `unistd.h` | `fs/read_write.c` |
| [`pread64`](./read-pread-readahead.md)   | 17         | `unistd.h` | `fs/read_write.c` |
| [`readahead`](./read-pread-readahead.md) | 187        | `fcntl.h`  | `fs/read_write.c` |

