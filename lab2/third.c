#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <linux/ptrace.h>
#include <linux/elf.h>

#define MAX_SYSCALLS 512
#define SYSCALL_FILE "/usr/include/asm-generic/unistd.h"

typedef struct {
    char name[64];
} syscall_entry;

syscall_entry syscall_list[MAX_SYSCALLS];

void load_syscall_names(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen syscall header");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long num;
        char name[64];

        if (sscanf(line, "#define __NR_%64s %lu", name, &num) == 2 || sscanf(line, "#define __NR3264_%64s %lu", name, &num) == 2) {
            if (num < MAX_SYSCALLS) {
                strncpy(syscall_list[num].name, name, sizeof(syscall_list[num].name) - 1);
                syscall_list[num].name[sizeof(syscall_list[num].name) - 1] = '\0';
            }
        }
    }

    fclose(fp);
}

const char* get_syscall_name(unsigned long num) {
    if (num < MAX_SYSCALLS) {
        return syscall_list[num].name;
    }
    return "unknown";
}

int main(int argc, char *argv[]) {
    pid_t child_pid;
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program_to_run>\n", argv[0]);
        return 1;
    }

    load_syscall_names(SYSCALL_FILE);

    int status;
    struct user_pt_regs regs;
    child_pid = fork();

    if (child_pid == 0) {
        // Дочерний процесс
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("ptrace");
            exit(1);
        }
        execl(argv[1], argv[1], NULL);
        perror("execl");
        exit(1);
    }
    else if (child_pid > 0) {
        // Родитель
        waitpid(child_pid, &status, 0);
        ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);

        while (1) {
            waitpid(child_pid, &status, 0);

            if (WIFEXITED(status)) {
                break;
            }

            struct iovec iov = {
                .iov_base = &regs,
                .iov_len = sizeof(regs),
            };
            if (ptrace(PTRACE_GETREGSET, child_pid, (void*)NT_PRSTATUS, &iov) == -1) {
                perror("ptrace GETREGSET");
                break;
            }

            unsigned long syscall_num = regs.regs[8];
            printf("[ENTER] Syscall %lu (%s)\n", syscall_num, get_syscall_name(syscall_num));

            ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);

            waitpid(child_pid, &status, 0);

            if (WIFEXITED(status)) {
                break;
            }
            if (ptrace(PTRACE_GETREGSET, child_pid, (void*)NT_PRSTATUS, &iov) == -1) {
                perror("ptrace GETREGSET");
                break;
            }





            long retval = regs.regs[0];
            printf("[EXIT ] Return: %ld\n", retval);

            ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);
        }
    } else {
        perror("fork");
        return 1;
    }

    return 0;
}


/**
Системный вызов ptrace(2) позволяет одному процессу контролировать выполнение другого
less /usr/include/asm-generic/unistd.h

48  - faccessat        - Проверка прав доступа к файлу с использованием файлового дескриптора директории
56  - openat           - Открыть файл относительно директории (например, `/proc/self/fd`)
57  - close            - Закрыть файловый дескриптор
62  - lseek            - Установить позицию чтения/записи в файле
63  - read             - Прочитать данные из файла или другого дескриптора
64  - write            - Записать данные в файл или другой дескриптор
80  - fstat            - Получить информацию о файле по дескриптору
94  - exit_group       - Завершить все потоки процесса
96  - set_tid_address  - Установить адрес переменной, содержащей TID потока (для NPTL)
99  - set_robust_list  - Установить список "устойчивых" мьютексов (для правильного восстановления после креша)
214 - brk              - Изменить конец сегмента данных (управление heap)
215 - munmap           - Освободить отображённую область памяти
222 - mmap             - Отобразить файл или память в адресное пространство процесса
226 - mprotect         - Изменить права доступа к области памяти
261 - prlimit64        - Ограничения ресурсов процесса (как `ulimit`, но на уровне процесса)
293 - rseq             - Restartable Sequences — оптимизация атомарных операций на многопроцессорных системах

**/
