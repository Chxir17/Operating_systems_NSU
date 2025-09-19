#include <stdio.h>
#include <unistd.h>

int main() {
    printf("pid:%d\n", getpid());
    sleep(1);
    execvp("./main", NULL);
    perror("execvp");
    printf("Hello world \n");
    return 0;
}