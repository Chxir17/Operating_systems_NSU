#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <limits.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

void resursiveStackArray() {
    char array[777];
    printf("Array pointer: %p\n", (void *) &array);
    sleep(2);
    resursiveStackArray();
}

_Noreturn void resursiveHeap() {
    while (1) {
        char *array = malloc(10000);
        printf("Array pointer: %p\n", (void *) &array);
        sleep(2);
        free(array);
    }
}

void createAdd() {
    long long regionSize = 10 * PAGE_SIZE;
    printf("Create new region address.\n");
    void *regionAddr = mmap(NULL, regionSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (regionAddr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    printf("Mapped a region at address %p with size %lld bytes.\n", regionAddr, regionSize);
    sleep(10);
    printf("Unmapping region.\n");
    if (munmap(regionAddr, regionSize) == -1) {
        perror("munmap");
        exit(1);
    }
}

void createAddChange() {
    long long regionSize = 10 * PAGE_SIZE;
    void *regionAddr = mmap(NULL, regionSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (regionAddr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    printf("Mapped a region at address %p with size %lld bytes\n", regionAddr, regionSize);

    char *message = "dep part2";
    printf("Write to region: %s\n", message);
    sprintf(regionAddr, "%s", message);
    printf("Read from region: %s\n", (char *) regionAddr);
    printf("Change mprotect. \n");
    if (mprotect(regionAddr, regionSize, PROT_WRITE) == -1) {
        perror("mprotect");
        exit(1);
    }
    printf("Trying to read from region after mprotect(PROT_WRITE):\n");
    printf("%s\n", (char *) regionAddr);

    if (mprotect(regionAddr, regionSize, PROT_READ) == -1) {
        perror("mprotect");
        exit(1);
    }

    printf("Trying to write to region after mprotect(PROT_READ):\n");
    sprintf(regionAddr, "%s", message);
    if (munmap(regionAddr, regionSize) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
}

void handleSigsegv() {
    write(0, "Received SIGSEGV signal.", 24);
    exit(1);
}

void createAddChangeHandled() {
    long long regionSize = 10 * PAGE_SIZE;
    void *regionAddr = mmap(NULL, regionSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (regionAddr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    printf("Mapped a region at address %p with size %lld bytes\n", regionAddr, regionSize);

    char *message = "Hdep part3";
    printf("Write to region: %s\n", message);
    sprintf(regionAddr, "%s", message);
    printf("Read from region: %s\n", (char *) regionAddr);

    printf("Change mprotect. \n");
    if (mprotect(regionAddr, regionSize, PROT_WRITE) == -1) {
        perror("mprotect");
        exit(1);
    }
    signal(SIGSEGV, handleSigsegv);

    printf("Trying to read from region after mprotect(PROT_WRITE):\n");
    printf("%s\n", (char *) regionAddr);

    if (mprotect(regionAddr, regionSize, PROT_READ) == -1) {
        perror("mprotect");
        exit(1);
    }

    printf("Trying to write to region after mprotect(PROT_READ):\n");
    sprintf(regionAddr, "%s", message);
    // sigsegv here
    if (munmap(regionAddr, regionSize) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
}

void createAddPart() {
    long long regionSize = 10 * PAGE_SIZE;
    printf("Create new region address. \n");
    void *regionAddr = mmap(NULL, regionSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (regionAddr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    printf("Mapped a region at address %p with size %lld bytes. \n", regionAddr, regionSize);
    sleep(10);

    long long partialSize = 3 * PAGE_SIZE;
    void *partialAddr = regionAddr + 3 * PAGE_SIZE;
    printf("Unmapping a part of the region at addr %p with size %lld bytes. \n", partialAddr, partialSize);
    if (munmap(partialAddr, partialSize) == -1) {
        perror("munmap");
        exit(1);
    }
    sleep(10);

    printf("Unmapping the remaining part of the region. \n");
    if (munmap(regionAddr, regionSize) == -1) {
        perror("munmap");
        exit(1);
    }
    sleep(10);
}

int main() {
    printf("My pid: %d \n", getpid());
    sleep(10);
//    resursiveStackArray();
//    resursiveHeap();
//    createAdd();
//    createAddChange();
//    createAddChangeHandled();
//    createAddPart();
    return 0;
}
