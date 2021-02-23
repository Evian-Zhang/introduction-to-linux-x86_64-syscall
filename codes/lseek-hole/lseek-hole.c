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
