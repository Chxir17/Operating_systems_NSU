#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main() {
    void *ptr = malloc(2000*sizeof(int));
    if (ptr == NULL) {
        perror("malloc");
        return 1;
    }

    memset(ptr, 0, 2000*sizeof(int));
    printf("PID: %d\n", getpid());
    sleep(60);
    free(ptr);
    return 0;
}
