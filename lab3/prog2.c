#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>
#include <errno.h>

#define SIZE 4096


enum ERRORS {
    SUCCESS = 0,
    USAGE_ERROR = 1
};

void myMkdir(char *path, mode_t mode) {
    if (mkdir(path, mode) == -1) {
        perror("mkdir error");
    }
}

void myLs(char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir error");
        return;
    }
    struct dirent *files;
    while ((files = readdir(dir)) != NULL) {
        printf("%s\n", files->d_name);
    }
    closedir(dir);
}

int myRmdir(char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir error");
        return -1;
    }

    struct dirent *files;
    int success = 0;
    while ((files = readdir(dir))) {
        if (!strcmp(files->d_name, ".") || !strcmp(files->d_name, "..")) {
          continue;
        }
        char fullPath[SIZE];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, files->d_name);
        struct stat st;
        if (stat(fullPath, &st) == -1) {
            perror("stat error");
            success = -1;
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (myRmdir(fullPath) == -1) success = -1;
        }
        else {
            if (unlink(fullPath) == -1) {
                perror("unlink error");
                success = -1;
            }
        }
    }
    closedir(dir);
    if (success == 0 && rmdir(path) == -1) {
        perror("rmdir error");
        return -1;
    }
    return success;
}

void myTouch(char *path) {
    FILE *file = fopen(path, "w");
    if (!file) {
        perror("fopen error");
        return;
    }
    fclose(file);
}

void myCat(char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("fopen error");
        return;
    }

    int c;
    while ((c = fgetc(file)) != EOF) {
        putchar(c);
    }
    fclose(file);
}

void myRm(char *path) {
    if (remove(path) == -1) {
        perror("remove error");
    }
}

void mySymLn(char *destPath, char *linkPath) {
    if (symlink(destPath, linkPath) == -1) {
        perror("symlink error");
    }
}

void myReadlink(char *path) {
    char buf[SIZE];
    long len = readlink(path, buf, sizeof(buf) - 1);
    if (len == -1) {
        perror("readlink error");
        return;
    }
    buf[len] = '\0';
    printf("%s\n", buf);
}

void catLinkContent(char *path) {
    char buf[SIZE];
    long len = readlink(path, buf, sizeof(buf) - 1);
    if (len == -1) {
        perror("readlink error");
        return;
    }
    buf[len] = '\0';
    myCat(buf);
}

void myLn(char *destPath, char *linkPath) {
    if (link(destPath, linkPath) == -1) {
        perror("link error");
    }
}

void myLsl(char *path) {
    struct stat sb;
    if (stat(path, &sb) == -1) {
        perror("stat error");
        return;
    }

    printf("Permissions: %o\n", sb.st_mode & 0777);
    printf("File Permissions: \t");
    printf((S_ISDIR(sb.st_mode)) ? "d" : "-");
    printf((sb.st_mode & S_IRUSR) ? "r" : "-");
    printf((sb.st_mode & S_IWUSR) ? "w" : "-");
    printf((sb.st_mode & S_IXUSR) ? "x" : "-");
    printf((sb.st_mode & S_IRGRP) ? "r" : "-");
    printf((sb.st_mode & S_IWGRP) ? "w" : "-");
    printf((sb.st_mode & S_IXGRP) ? "x" : "-");
    printf((sb.st_mode & S_IROTH) ? "r" : "-");
    printf((sb.st_mode & S_IWOTH) ? "w" : "-");
    printf((sb.st_mode & S_IXOTH) ? "x" : "-");
    printf("\n\n");

    printf("Number of hard links: %ld\n", (long)sb.st_nlink);
}

void myChmod(char *path, mode_t mode) {
    if (chmod(path, mode) == -1) {
        perror("chmod error");
    }
}


int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ./command <path> [...]\n Commands: mkdir, ls, rmdir, touch, cat, rm, lnSym, readlink,\n catLinkContent, rmSymLink, ln, rmHardLink, lsl, chmod\n");
        return USAGE_ERROR;
    }
    char *action = basename(argv[0]);
    char *path = argv[1];

    if (strcmp(action, "mkdir") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: mkdir <path> <mode>\n");
            return USAGE_ERROR;
        }
        mode_t mode = strtol(argv[2], NULL, 8);
        myMkdir(path, mode);
    }
    else if (strcmp(action, "ls") == 0) {
        myLs(path);
    }
    else if (strcmp(action, "rmdir") == 0) {
        myRmdir(path);
    }
    else if (strcmp(action, "touch") == 0) {
        myTouch(path);
    }
    else if (strcmp(action, "cat") == 0) {
        myCat(path);
    }
    else if (strcmp(action, "rm") == 0) {
        myRm(path);
    }
    else if (strcmp(action, "lnSym") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: lnSym <target> <link>\n");
            return USAGE_ERROR;
        }
        char *destPath = argv[1];
        char *linkPath = argv[2];
        mySymLn(destPath, linkPath);
    }
    else if (strcmp(action, "readlink") == 0) {
        myReadlink(path);
    }
    else if (strcmp(action, "catLinkContent") == 0) {
        catLinkContent(path);
    }
    else if (strcmp(action, "rmSymLink") == 0) {
        myRm(path);
    }
    else if (strcmp(action, "ln") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: ln <target> <link>\n");
            return USAGE_ERROR;
        }
        char *destPath = argv[1];
        char *linkPath = argv[2];
        myLn(destPath, linkPath);
    }
    else if (strcmp(action, "rmHardLink") == 0) {
        myRm(path);
    }
    else if (strcmp(action, "lsl") == 0) {
        myLsl(path);
    }
    else if (strcmp(action, "chmod") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: chmod <path> <mode>\n");
            return USAGE_ERROR;
        }
        mode_t mode = strtol(argv[2], NULL, 8);
        myChmod(path, mode);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", action);
        return USAGE_ERROR;
    }
    return SUCCESS;
}
