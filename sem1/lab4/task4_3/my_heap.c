#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HEAP 10485760
void *heapPtr = NULL;

typedef struct block {
    unsigned long long size;
    struct block *next;
    char free;
} block;
block *heap;

void *myMalloc(unsigned long long size) {
    block *current = heap;
    while (current != NULL) {
        if (current->free && current->size >= size) {
            if (current->size > size + sizeof(block)) {
                block *newMem = (block *) ((char *) current + sizeof(block) + size);
                newMem->size = current->size - size - sizeof(block);
                newMem->free = 1;
                newMem->next = current->next;
                current->size = size;
                current->free = 0;
                current->next = newMem;
            }
            else {
                current->free = 0;
            }
            return (char *) current + sizeof(block);
        }
        current = current->next;
    }
    return NULL;
}

void mergeBlocks() {
    block *current = heap;
    while (current != NULL && current->next != NULL) {
        if (current->free && current->next->free) {
            current->size += current->next->size + sizeof(block);
            current->next = current->next->next;
        }
        else {
            current = current->next;
        }
    }
}

void myFree(void *memory) {
    if (memory == NULL) {
        return;
    }
    block *current = (block *) ((char *) memory - sizeof(block));
    current->free = 1;
    memset(memory, 1, current->size);
    mergeBlocks();
}


void initialize() {
    heapPtr = mmap(NULL, HEAP, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (heapPtr == MAP_FAILED) {
        perror("init mmap");
        exit(1);
    }
    heap = (block *) heapPtr;
    heap->size = HEAP - sizeof(block);
    heap->free = 1;
    heap->next = NULL;
}

void print(void *ptr, unsigned long long size) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long long i = 0; i < size; i++) {
        printf("%X ", p[i]);
        if ((i + 1) % 16 == 0){
            printf("\n");
        }
    }
    if (size % 16 != 0){
        printf("\n");
    }
}


int main() {
    initialize();
    char *ptr1 = myMalloc(1000);
    if (ptr1) {
        strcpy(ptr1, "Hello");
        fprintf(stdout, "ptr1: %s %p\n", ptr1, ptr1);
    }

    char *ptr2 = myMalloc(2000);
    if (ptr2) {
        strcpy(ptr2, "Hello2");
        fprintf(stdout, "ptr2: %s %p\n", ptr2, ptr2);
    }
    char *ptr3 = myMalloc(HEAP);
    fprintf(stdout, "ptr3: %s %p \n", ptr3, ptr3);
    myFree(ptr1);
    print(heap, ((block *)((char *)ptr1 - sizeof(block)))->size);
    myFree(ptr2);
    char *ptr4 = myMalloc(40000);
    char *ptr5 = myMalloc(50000);
    if (ptr4) {
        strcpy(ptr4, "Hi");
        fprintf(stdout, "ptr4: %s %p\n", ptr4, ptr4);
    }
    if (ptr5) {
        strcpy(ptr5, "QQ");
        fprintf(stdout, "ptr5: %s %p\n", ptr5, ptr5);
    }

    munmap(heapPtr, HEAP);
    exit(0);
}
