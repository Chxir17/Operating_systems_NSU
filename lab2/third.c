#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void run_child(const char *program_name) {

}

int main(int argc, char *argv[]) {
    pid_t child_pid;
    if (argc < 2) {
        fprintf(stderr, "Use: ./<this programm> <program to run>\n");
        return 1;
    }
    printf("Prog to run '%s'\n", argv[1]);
    child_pid = fork();


    if (child_pid == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("ptrace");
            exit(1);
        }
        int execl_err = execl(argv[1], argv[1], NULL);
        if (execl_err == -1) {
            perror("execv");
            exit(1);
        }
    }

    else if (child_pid > 0) {

        int wait_status;
        unsigned long long syscall;
        wait(&wait_status);
        while (WIFSTOPPED(wait_status)) {
            struct user_pt_regs regs;
            if (ptrace(PTRACE_SYSCALL, child_pid, 0, 0) < 0) {
                perror("ptrace");
                exit(1);
            }
            wait(&wait_status);
            if (WIFEXITED(wait_status)) {
                break;
            }
            if (ptrace(PTRACE_GETREGS, child_pid, NULL, &regs) < 0) {
                perror("ptrace");
                exit(1);
            }
            syscall = regs.regs[8];  // arm syscaall number
            printf("Current syscall: %llu\n", syscall);
            if (ptrace(PTRACE_SYSCALL, child_pid, 0, 0) < 0) {
                perror("ptrace");
                exit(1);
            }
            wait(&wait_status);
        }
    }
    else {
        perror("fork");
        return 1;
    }
    return 0;
}