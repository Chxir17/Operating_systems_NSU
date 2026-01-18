#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    int file = open("test.txt", O_CREAT | O_RDWR, 0666);
    if (file == -1) {
        perror("open");
        return 1;
    }

    const char *text = "Hello\n";
    if (write(file, text, 6) == -1) {
        perror("write");
        close(file);
        return 1;
    }

    if (lseek(file, 0, SEEK_SET) == -1) {
        perror("lseek");
        close(file);
        return 1;
    }

    char buffer[7];
    if (read(file, buffer, 6) == -1) {
        perror("read");
        close(file);
        return 1;
    }

    if (write(1, buffer, 6) == -1) {
        perror("write");
        close(file);
        return 1;
    }
    close(file);
    return 0;
}