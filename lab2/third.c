#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>      // user_pt_regs для ARM64
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>

// Для ARM64 используем struct user_pt_regs вместо struct user_regs_struct
#include <linux/ptrace.h>
#include <linux/elf.h>

int main(int argc, char *argv[]) {
    pid_t child_pid;
    if (argc < 2) {
        fprintf(stderr, "Use: ./<this programm> <program to run>\n");
        return 1;
    }
    printf("Prog to run '%s'\n", argv[1]);
    int status;
    struct user_pt_regs regs; //регистры ARM64, получаемые от ptrace
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
        // Родитель
        waitpid(child_pid, &status, 0); // Ждём SIGSTOP

        ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL); //продолжаем до следующего syscall

        while (1) {
            waitpid(child_pid, &status, 0);
            if (WIFEXITED(status)) {
                break;
            }
            // Вход в системный вызов
            struct iovec iov = {
                .iov_base = &regs,
                .iov_len = sizeof(regs),
            };
            if (ptrace(PTRACE_GETREGSET, child_pid, (void*)NT_PRSTATUS, &iov) == -1) { // NT_PRSTATUS (with numerical value 1) usually result in reading of general-purpose registers.
                perror("ptrace GETREGSET");
                break;
            }

            unsigned long syscall = regs.regs[8]; //syscall на arm64
            printf("Syscall number: %lu\n", syscall);

            ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);

            // Ждём выход из системного вызова
            waitpid(child_pid, &status, 0);
            if (WIFEXITED(status)) {
                break;
            }
            ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);//разрешаем дочернему процессу продолжить до выхода из системного вызова.
        }
    }
    else {
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
