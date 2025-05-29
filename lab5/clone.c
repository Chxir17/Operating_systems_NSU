#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>

void recursion(int iter) {
    char str[] = "!!!!!!!!!!!!!!";
    printf("next iter\n");
    sleep(5);
    if (iter > 0){
        recursion(iter - 1);
    }
}

int entryPoint(void* arg) {

    printf("PID = %d, PPID = %d\n", getpid(), getppid());
    recursion(10);
    return 0;
}

int main() {
    int fd = open("stack.bin", O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(fd, 1024 * 16) == -1) {//установка размера файла в размер
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    void* stack = mmap(NULL, 1024 * 16, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);//указатель на область
    if (stack == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    void* stackTop = (char*)stack + 1024 * 16;

    pid_t pid = clone(entryPoint, stackTop, SIGCHLD, NULL); //сигнал при завершении
    if (pid == -1) {
        perror("clone");
        exit(EXIT_FAILURE);
    }

    int status;
    pid_t wpid = wait(&status);

    if (wpid == -1) {
        perror("wait error");
        return 1;
    }

    if (WIFEXITED(status)) {
        int exitСode = WEXITSTATUS(status);
        printf("child exit code: %d\n", exitСode);
    }

    munmap(stack,  1024 * 16);
    close(fd);
    return 0;
}
