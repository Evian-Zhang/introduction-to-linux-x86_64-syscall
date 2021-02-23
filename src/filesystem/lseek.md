# `lseek`系统调用

## 系统调用号

8

## 函数签名

### 内核接口

```c
asmlinkage long sys_lseek(unsigned int fd, off_t offset, unsigned int whence);
```

### glibc封装

```c
#include <sys/types.h>
#include <unistd.h>
off_t lseek(int fd, off_t offset, int whence);
```

## 简介

对于支持seek的文件类型，在内核的文件描述中，会有一个“文件偏移”（file offset）。这个的作用是标记后续的`read`和`write`的起点。而`lseek`系统调用的作用就是操作这个文件偏移。

对于`fd`对应的文件，`lseek`的功能根据`whence`的不同而不同。`whence`可能的值包括：

* `SEEK_SET`

    将文件偏移置于`offset`

* `SEEK_CUR`

    将文件偏移置于距当前偏移的`offset`字节。

    `offset`为正则为当前偏移之后，`offset`为负则为当前偏移之前

* `SEEK_END`

    将文件偏移置于距文件末尾的`offset`字节。

    `offset`为负则为文件末尾之前。`offset`可以为正，表示在文件末尾之后的`offset`个字节。如果在此处写入，那么使用`read`读取之前文件末尾至当前偏移，将会得到被`\0`填充的区域（被称为洞）。
    
    假设当前文件长度为5字节，我们使用`lseek`，从第8字节的位置开始写入。然后使用`read`读取6到7字节的内容，则内容为`\0`填充的2字节区域。

* `SEEK_DATA`

    将文件偏移置于从`offset`开始，第一个包含数据的字节。

    如果我们使用`SEEK_END`制造了洞，且当前文件偏移在洞中，则`SEEK_DATA`将会将我们的文件偏移移动到之后第一个有数据的区域。

* `SEEK_HOLE`

    将文件偏移置于从`offset`开始，第一个洞的开始字节。

    如果该文件没有洞，则将置于文件末尾。

`lseek`的返回值为进行修改文件偏移的操作之后，当前的文件偏移。

我们可以用[这个示例](https://github.com/Evian-Zhang/introduction-to-linux-x86_64-syscall/tree/master/codes/lseek-hole)来简单了解一下`lseek`的功能：

```c
#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main() {
    int fd = open("./text.txt", O_RDWR);
    int end_offset = lseek(fd, 0, SEEK_END);
    lseek(fd, 4, SEEK_END);
    write(fd, "123", 3);
    char buf[4];
    lseek(fd, end_offset, SEEK_SET);
    read(fd, buf, 4);
    for (int i = 0; i < 4; i++) {
        printf("%d", buf[i]);
    }
    printf("\n");

    int at_hole = lseek(fd, end_offset + 2, SEEK_SET);
    int next_data = lseek(fd, at_hole, SEEK_DATA);
    printf("Current offset %d at hole, move to %d with SEEK_DATA\n", at_hole, next_data);

    int at_data = lseek(fd, end_offset - 2, SEEK_SET);
    int next_hole = lseek(fd, at_data, SEEK_HOLE);
    printf("Current offset %d at data, move to %d with SEEK_HOLE\n", at_data, next_hole);

    return 0;
}
```

其中`text.txt`的内容长度为6字节。

因此，我们的步骤是

1. 将文件偏移移动到末尾之后4字节，当前文件偏移为10
2. 写入数据
3. 将文件偏移移动到6
4. 读入4字节数据。这4字节由于在“洞“中，所以均为0
5. 将文件偏移移动到8
6. 使用`SEEK_DATA`找到下一个有数据的区域，文件偏移为10
7. 将文件偏移移动到4
8. 使用`SEEK_HOLE`找到下一个洞的区域，文件偏移为6

但是，由于不同的文件系统对`SEEK_DATA`和`SEEK_HOLE`的支持不同，上述行为在不同文件系统下可能会不同。最简单的实现就是，`SEEK_DATA`不改变文件偏移，`SEEK_HOLE`移动到文件末尾，也就是把“洞”也看作正常的数据。

## 实现

`lseek`的实现位于`fs/read_write.c`，其最终调用的是`vfs_llseek`：

```c
loff_t vfs_llseek(struct file *file, loff_t offset, int whence)
{
	loff_t (*fn)(struct file *, loff_t, int);

	fn = no_llseek;
	if (file->f_mode & FMODE_LSEEK) {
		if (file->f_op->llseek)
			fn = file->f_op->llseek;
	}
	return fn(file, offset, whence);
}
```

而我们来看看比较常见的EXT4文件系统的实现，其位于`fs/ext4/file.c`:

```c
loff_t ext4_llseek(struct file *file, loff_t offset, int whence)
{
	struct inode *inode = file->f_mapping->host;
	loff_t maxbytes;

	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)))
		maxbytes = EXT4_SB(inode->i_sb)->s_bitmap_maxbytes;
	else
		maxbytes = inode->i_sb->s_maxbytes;

	switch (whence) {
	default:
		return generic_file_llseek_size(file, offset, whence,
						maxbytes, i_size_read(inode));
	case SEEK_HOLE:
		inode_lock_shared(inode);
		offset = iomap_seek_hole(inode, offset, &ext4_iomap_ops);
		inode_unlock_shared(inode);
		break;
	case SEEK_DATA:
		inode_lock_shared(inode);
		offset = iomap_seek_data(inode, offset, &ext4_iomap_ops);
		inode_unlock_shared(inode);
		break;
	}

	if (offset < 0)
		return offset;
	return vfs_setpos(file, offset, maxbytes);
}
```

对于`SEEK_HOLE`和`SEEK_DATA`，EXT4特殊考虑了，实现了正确的行为。对于一般的操作，则是使用`fs/read_write`里的`generic_file_llseek_size`函数：

```c
loff_t
generic_file_llseek_size(struct file *file, loff_t offset, int whence,
		loff_t maxsize, loff_t eof)
{
	switch (whence) {
	case SEEK_END:
		offset += eof;
		break;
	case SEEK_CUR:
		/*
		 * Here we special-case the lseek(fd, 0, SEEK_CUR)
		 * position-querying operation.  Avoid rewriting the "same"
		 * f_pos value back to the file because a concurrent read(),
		 * write() or lseek() might have altered it
		 */
		if (offset == 0)
			return file->f_pos;
		/*
		 * f_lock protects against read/modify/write race with other
		 * SEEK_CURs. Note that parallel writes and reads behave
		 * like SEEK_SET.
		 */
		spin_lock(&file->f_lock);
		offset = vfs_setpos(file, file->f_pos + offset, maxsize);
		spin_unlock(&file->f_lock);
		return offset;
	case SEEK_DATA:
		/*
		 * In the generic case the entire file is data, so as long as
		 * offset isn't at the end of the file then the offset is data.
		 */
		if ((unsigned long long)offset >= eof)
			return -ENXIO;
		break;
	case SEEK_HOLE:
		/*
		 * There is a virtual hole at the end of the file, so as long as
		 * offset isn't i_size or larger, return i_size.
		 */
		if ((unsigned long long)offset >= eof)
			return -ENXIO;
		offset = eof;
		break;
	}

	return vfs_setpos(file, offset, maxsize);
}
```

这是默认实现的行为，对于`SEEK_SET`, `SEEK_CUR`和`SEEK_END`，清楚地实现了它们的功能；对于`SEEK_DATA`，则默认不改变文件偏移，`SEEK_HOLE`则会移动到文件末尾。
