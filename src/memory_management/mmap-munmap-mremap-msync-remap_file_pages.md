# `mmap`, `munmap`, `mremap`, `msync`, `remap_file_pages`系统调用

## `mmap`

### 系统调用号

9

### 函数签名

#### 内核接口

```c
asmlinkage long sys_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags, unsigned long fd, off_t pgoff);
```

#### glibc封装

```c
#include <sys/mman.h>
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```

### 简介

`mmap`做了一个特别神奇的事：把硬盘上的文件与内存之间建立映射。首先，我们来看看最终效果。假设我们有一个二进制文件`data.bin`，其内容为（16进制）：

```plaintext
11 45 14 19 19 81 00
```

我们通过`mmap`，将这个文件映射到了内存中从`0x10000`开始的区域。接下来，如果我们的程序从内存`0x10002`读取2字节的内容，将得到`0x1914`这个数字（小端序）。

也就是说，我们没有借助`read`系统调用，而是直接对某个内存区域进行读取，就能读取到硬盘上的文件的内容。

接着我们来看看其参数。

总得来说，`mmap`的行为是，在`flags`的控制下，将描述符为`fd`的文件中，从`offset`位置开始，`length`个字节映射到地址为`addr`的内存空间中，并设置其内存保护为`prot`。

在一般情况下，地址和长度应遵守这样的限制：

* 如果`addr`为`NULL`，则内核自己选择适当的内存地址进行映射。如果其不为`NULL`，则内核以`addr`的值为参考，选择一个适当的内存地址进行映射（一般是之后最近的页边界）。
* `offset`应为页大小的倍数。
* `length`应大于0。如果被映射的大小不是页大小的整数倍，则剩下的页的部分会被0填充。

控制`mmap`行为的核心为`flags`。其可以包含以下标志位：

* 核心标志位

    以下三个标志位必须且只能包含一个。
    * `MAP_SHARED`

        建立一个共享的映射。

        如果在该进程中，对该映射后的内存区域进行修改，那么在别的进程中，如果其使用了`mmap`，将同一个文件也进行了映射，那么可以同步看见该修改。同时被映射的文件也会被修改。

    * `MAP_SHARED_VALIDATE`

        行为和`MAP_SHARED`类似。但是会核验`flags`，如果其包含了未知的标志位，将报错`EOPNOTSUPP`。
    
    * `MAP_PRIVATE`

        建立一个私有的写时复制（copy-on-write）的映射。

        对该映射的内存区域进行的修改不会同步到其他进程中，也不会修改硬盘里相应的文件。

* 附加标志位

    除了三个必要的标志位之外，还有一些标志位也可以被包含。其主要包括

    * `MAP_ANONYMOUS`

        忽略`fd`，被映射的内存区域将被初始化为0。

        此时`fd`应为-1，`offset`应为0。
    
    * `MAP_FIXED`

        将`addr`看作确切的地址，而非一个参考。

        内核将准确地将文件映射到从`addr`开始的内存区域。如果这个映射与之前已经存在的内存映射有重合，则重合的部分将被新的映射覆盖。

    * `MAP_FIXED_NOREPLACE`

        行为和`MAP_FIXED`类似，但不会覆盖已有的内存映射。如果与已有的内存映射有重合，那么将直接返回错误`EEXIST`。
    
`prot`参数则是控制映射的内存区域的内存保护，其可能的值包括

* `PROC_EXEC`

    页可执行

* `PROC_READ`

    页可读

* `PROC_WRITE`

    页可写

* `PROC_NONE`

    页不可访问

使用`mmap`读取文件的好处在于：使用`read`读取文件时，会先将文件的内容从硬盘上复制到内核的内存空间中，然后再由内核将数据复制到用户的内存空间中。但使用`mmap`时，文件的内容是可以直接复制到用户的内存空间中的。

### 实现

`mmap`的实现位于Linux内核源码的`arch/x86/kernel/sys_x86_64.c`，其直接调用了位于`mm/mmap.c`的`ksys_mmap_pgoff`函数。在经过了复杂的函数链之后，我们可以发现，其最终是调用的位于`include/linux/fs.h`的`call_mmap`函数：

```c
static inline int call_mmap(struct file *file, struct vm_area_struct *vma)
{
	return file->f_op->mmap(file, vma);
}
```

其中`struct vm_area_struct`这个结构体，表示一块虚拟内存，其包含一个类型为`struct vm_operations_struct`的字段`vm_ops`，表示对虚拟内存的操作，其定义在`include/linux/mm.h`:

```c
/*
 * These are the virtual MM functions - opening of an area, closing and
 * unmapping it (needed to keep files on disk up-to-date etc), pointer
 * to the functions called when a no-page or a wp-page exception occurs.
 */
struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	int (*split)(struct vm_area_struct * area, unsigned long addr);
	int (*mremap)(struct vm_area_struct * area);
	vm_fault_t (*fault)(struct vm_fault *vmf);
	vm_fault_t (*huge_fault)(struct vm_fault *vmf,
			enum page_entry_size pe_size);
	void (*map_pages)(struct vm_fault *vmf,
			pgoff_t start_pgoff, pgoff_t end_pgoff);
	unsigned long (*pagesize)(struct vm_area_struct * area);

	/* notification that a previously read-only page is about to become
	 * writable, if an error is returned it will cause a SIGBUS */
	vm_fault_t (*page_mkwrite)(struct vm_fault *vmf);

	/* same as page_mkwrite when using VM_PFNMAP|VM_MIXEDMAP */
	vm_fault_t (*pfn_mkwrite)(struct vm_fault *vmf);

	/* called by access_process_vm when get_user_pages() fails, typically
	 * for use by special VMAs that can switch between memory and hardware
	 */
	int (*access)(struct vm_area_struct *vma, unsigned long addr,
		      void *buf, int len, int write);

	/* Called by the /proc/PID/maps code to ask the vma whether it
	 * has a special name.  Returning non-NULL will also cause this
	 * vma to be dumped unconditionally. */
	const char *(*name)(struct vm_area_struct *vma);

#ifdef CONFIG_NUMA
	/*
	 * set_policy() op must add a reference to any non-NULL @new mempolicy
	 * to hold the policy upon return.  Caller should pass NULL @new to
	 * remove a policy and fall back to surrounding context--i.e. do not
	 * install a MPOL_DEFAULT policy, nor the task or system default
	 * mempolicy.
	 */
	int (*set_policy)(struct vm_area_struct *vma, struct mempolicy *new);

	/*
	 * get_policy() op must add reference [mpol_get()] to any policy at
	 * (vma,addr) marked as MPOL_SHARED.  The shared policy infrastructure
	 * in mm/mempolicy.c will do this automatically.
	 * get_policy() must NOT add a ref if the policy at (vma,addr) is not
	 * marked as MPOL_SHARED. vma policies are protected by the mmap_sem.
	 * If no [shared/vma] mempolicy exists at the addr, get_policy() op
	 * must return NULL--i.e., do not "fallback" to task or system default
	 * policy.
	 */
	struct mempolicy *(*get_policy)(struct vm_area_struct *vma,
					unsigned long addr);
#endif
	/*
	 * Called by vm_normal_page() for special PTEs to find the
	 * page for @addr.  This is useful if the default behavior
	 * (using pte_page()) would not find the correct page.
	 */
	struct page *(*find_special_page)(struct vm_area_struct *vma,
					  unsigned long addr);
};
```

和我们的`mmap`有关的字段是

```c
vm_fault_t (*fault)(struct vm_fault *vmf);
```

这个函数指针将在我们出现页错误的时候调用。

我们上面看到，`mmap`最终落实到了各个文件类型自己定义的`mmap`操作中。我们常见的EXT4文件系统中，这个操作为函数`ext4_file_mmap`:

```c
static const struct vm_operations_struct ext4_file_vm_ops = {
	.fault		= ext4_filemap_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite   = ext4_page_mkwrite,
};

static int ext4_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct inode *inode = file->f_mapping->host;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct dax_device *dax_dev = sbi->s_daxdev;

	if (unlikely(ext4_forced_shutdown(sbi)))
		return -EIO;

	/*
	 * We don't support synchronous mappings for non-DAX files and
	 * for DAX files if underneath dax_device is not synchronous.
	 */
	if (!daxdev_mapping_supported(vma, dax_dev))
		return -EOPNOTSUPP;

	file_accessed(file);
	if (IS_DAX(file_inode(file))) {
		vma->vm_ops = &ext4_dax_vm_ops;
		vma->vm_flags |= VM_HUGEPAGE;
	} else {
		vma->vm_ops = &ext4_file_vm_ops;
	}
	return 0;
}
```

可以看到，在通常情况下，是使用`ext4_filemap_fault`作为我们之前讲的`vm_operations_struct`中的`fault`字段。这个函数最终会被落实到`mm/filemap.c`的`filemap_fault`函数。在这个函数中，如果这个文件在内核的页缓存中，则直接去找那个页即可。如果没有，则调用`pagecache_get_page`，最终使用`__add_to_page_cache_locked`创建相应的页。
