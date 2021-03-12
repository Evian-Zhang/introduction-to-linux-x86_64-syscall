[序](README.md)

[基础知识](src/introduction.md)

# 文件系统

- [`open`, `openat`, `name_to_handle_at`, `open_by_handle_at`, `open_tree`](src/filesystem/open-openat-name_to_handle_at-open_by_handle_at-open_tree.md)
- [`close`](src/filesystem/close.md)
- [`read`, `pread64`, `readv`, `preadv`, `preadv2`](src/filesystem/read-pread64-readv-preadv-preadv2.md)
- [`write`, `pwrite64`, `writev`, `pwritev`, `pwritev2`](src/filesystem/write-pwrite64-writev-pwritev-pwritev2.md)
- [`lseek`](src/filesystem/lseek.md)
- [`poll`, `select`, `pselect6`, `ppoll`](src/filesystem/poll-select-pselect6-ppoll.md)
- [`epoll_create`, `epoll_wait`, `epoll_ctl`, `epoll_pwait`, `epoll_create1`](src/filesystem/epoll_create-epoll_wait-epoll_ctl-epoll_pwait-epoll_create1.md)
- [`stat`, `fstat`, `lstat`, `newfstatat`, `statx`](src/filesystem/stat-fstat-lstat-newfstatat-statx.md)
- [`eventfd`, `eventfd2`](src/filesystem/eventfd-eventfd2.md)

# 内存管理

- [`mmap`, `munmap`, `mremap`, `msync`, `remap_file_pages`](src/memory_management/mmap-munmap-mremap-msync-remap_file_pages.md)