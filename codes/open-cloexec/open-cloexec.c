#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("./text.txt", O_RDONLY); // open("./text.txt", O_RDONLY | O_CLOEXEC)
    if (fork() == 0) {
        // child process
        char *const argv[] = {"./child", NULL};
        execve("./child", argv, NULL); // fd left opened
    } else {
        // parent process
        sleep(30);
    }

    return 0;
}
