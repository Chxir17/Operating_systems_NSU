#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int global_var = 22;

int main() {
    int local_var = 203;
    printf("P_proc: global var %d and its address %p \n", global_var, &global_var);
    printf("P_proc: local var %d and its address %p \n", local_var, &local_var);
    pid_t pid_parent = getpid();
    printf("My pid: %d \n", pid_parent);
    pid_t pid_fork = fork();
    if (pid_fork == -1) {
        perror("Fork");
        return 1;
    }
    else if (pid_fork == 0) {
        printf("Child pid : %d \n", getpid());
        printf("Parent pid: %d \n", getppid());
        printf("C_proc: global var %d and its address %p \n", global_var, &global_var);
        printf("C_proc: local var %d and its address %p \n", local_var, &local_var);
        global_var = 20;
        local_var = 207;
        printf("C_proc: global var %d and its address %p \n", global_var, &global_var);
        printf("C_proc: local var %d and its address %p \n", local_var, &local_var);
        sleep(30);
        printf("Parent pid: %d \n", getppid());
        exit(2);
    }
    else {
        printf("P_proc: global var %d and its address %p \n", global_var, &global_var);
        printf("P_proc: local var %d and its address %p \n", local_var, &local_var);
        sleep(10);
        printf("Finish parent process \n");
    }
    printf("P_proc: global var %d and its address %p \n", global_var, &global_var);
    return 0;
}