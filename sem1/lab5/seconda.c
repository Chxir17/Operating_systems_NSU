#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("My pid: %d \n", getpid());
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    else if (pid == 0) {
        printf("Child pid: %d \n", getpid());
        sleep(2);
        printf("Child process finished | pid %d.\n", getpid());
        exit(0);
    }

    else {
        printf("Parent pid: %d.\n", getpid());
        sleep(15);
        printf("Parent process finished | pid %d.\n", getpid());
    }
    return 0;
}