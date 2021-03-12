#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

void test_file_offset(int fd) {
    char read_buf[16];
    char write_buf[] = "payload";
    printf("File offset is %zd.\n", lseek(fd, 0, SEEK_CUR));
    ssize_t read_length = read(fd, read_buf, 4);
    read_buf[read_length] = '\0';
    printf("Read %zd bytes: %s.\n", read_length, read_buf);
    printf("File offset is %zd.\n", lseek(fd, 0, SEEK_CUR));
    printf("Write %zd bytes.\n", write(fd, write_buf, 7));
    printf("File offset is %zd.\n", lseek(fd, 0, SEEK_CUR));
    read_length = read(fd, read_buf, 4);
    read_buf[read_length] = '\0';
    printf("Read %zd bytes: %s.\n", read_length, read_buf);
    printf("File offset is %zd.\n", lseek(fd, 0, SEEK_CUR));
}

int main() {
    int fd_with_append = open("./text.txt", O_RDWR | O_APPEND);
    printf("File opened with O_APPEND:\n");
    test_file_offset(fd_with_append);
    close(fd_with_append);
    int fd_without_append = open("./text.txt", O_RDWR);
    printf("File opened without O_APPEND:\n");
    test_file_offset(fd_without_append);
    close(fd_without_append);
    return 0;
}
