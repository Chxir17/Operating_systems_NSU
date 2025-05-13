#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

int globalInt = 777;
int globalVar;
const int globalConst = 777;

void locals() {
    printf("Locals:\n");
    int a;
    long long b;
    char c;
    float d;
    printf("int 'a' address: %p\n", (void *) &a);
    printf("long long 'b' address: %p\n", (void *) &b);
    printf("char 'c' address: %p\n", (void *) &c);
    printf("float 'd' address: %p\n", (void *) &d);
}

void statics() {
    printf("Statics:\n");
    static int a;
    static long long b;
    static char c;
    static float d;
    printf("int 'a' address: %p\n", (void *) &a);
    printf("long long 'b' address: %p\n", (void *) &b);
    printf("char 'c' address: %p\n", (void *) &c);
    printf("float 'd' address: %p\n", (void *) &d);
}

void constants() {
    printf("Consts\n");
    const int a;
    const long long b;
    const char c;
    const float d;
    printf("int 'a' address: %p\n", (void *) &a);
    printf("long long 'b' address: %p\n", (void *) &b);
    printf("char 'c' address: %p\n", (void *) &c);
    printf("float 'd' address: %p\n", (void *) &d);
}

void globals() {
    printf("Globals:\n");
    printf("int 'globalInt' address: %p\n", (void *) &globalInt);
    printf("int 'globalVar' address: %p\n", (void *) &globalVar);
    printf("int 'globalConst' address: %p\n", (void *) &globalConst);
}


int *variable() {
    int a = 777;
    printf("Variable address in func: %p\n", (void *) &a);
    return &a;
}

void buffer() {
    char *buffer = malloc(sizeof(char) * 77);
    assert(buffer != NULL);
    strcpy(buffer, "dep");
    printf("before free 1: %s\n", buffer);
    free(buffer);
    printf("after free 1: %s\n", buffer);

    char *buffer2 = malloc(sizeof(char) * 77);
    strcpy(buffer2, "dep again");
    printf("before free 2: %s \n", buffer2);

    char *midBuffer = (buffer2 + 77);

    printf("Content from the start of the second buffer: %s\n", buffer2);
    printf("Content in the middle of the second buffer: %s \n", midBuffer);
    free(midBuffer);
    printf("Content from the start of the second buffer after free: %s \n", buffer2);
    free(midBuffer);
}

void env() {
    printf("Environment variable value: %s\n", getenv("CASINO"));
    setenv("CASINO", "never dep again", 1);
    printf("New environment variable value: %s\n", getenv("CASINO"));
}

int main() {
    pid_t pid = getpid();
    printf("PID of this process : %d \n\n", pid);
    locals();
    statics();
    constants();
    globals();
    sleep(10);

    printf("Variable address in main: %p\n", (void *) variable());
    sleep(20);
    buffer();
    sleep(20);
    env();
    return 0;
}