#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
//qwe

#define SIZE 4096

void myMkdir(char *path, mode_t mode) {
    if (mkdir(path, mode) == -1) {
        printf("Error! Can't create directory.");
    }
}

void myLs(char *path) {
    DIR *dir;
    struct dirent *files;
    dir = opendir(path);
    if (dir) {
        while ((files = readdir(dir)) != NULL) {
            printf("%s\n", files->d_name);
        }
        closedir(dir);
    }
    else {
        printf("Error! Directory does not exist.");
    }
}
int myRmdir(char *path) {
    DIR *dir = opendir(path);
    int success = -1;
    if (dir) {
        struct dirent *files;
        long len, pathLen = strlen(path);
        success = 0;
        while ((files = readdir(dir)) && !success) {
            int success2 = -1;
            if (!strcmp(files->d_name, ".") || !strcmp(files->d_name, "..")) {
                continue;
            }

            char *fullPath;
            len = pathLen + strlen(files->d_name) + 2;
            fullPath = malloc(len);

            if (fullPath) {
                struct stat st;
                snprintf(fullPath, len, "%s/%s", path, files->d_name);
                if (!stat(fullPath, &st)) {
                    if (S_ISDIR(st.st_mode)){
                        success2 = myRmdir(fullPath);
                    }
                    else{
                        success2 = unlink(fullPath);
                    }
                }
                free(fullPath);
            }
            success = success2;
        }
        closedir(dir);
    }
    else {
        printf("Error! Directory does not exist.");
    }
    if (!success){
        success = rmdir(path);
    }
    return success;
}
void myTouch(char *path) {
    FILE *file = fopen(path, "w");
    fclose(file);
}
void myCat(char *path) {
    FILE *file = fopen(path, "r");
    int c;
    if (file) {
        while ((c = fgetc(file)) != EOF) {
            putchar(c);
        }
    }
    else {
        printf("Error! File doesn't exist.");
    }
    fclose(file);
}
void myRm(char *path) {
    if (remove(path) == -1) {
        printf("Error! Can't delete this file.");
    }
}
void mySymLn(char *destPath, char *linkPath) {
    symlink(destPath, linkPath);
}
void myReadlink(char *path) {
    char buf[SIZE];
    long len = readlink(path, buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        printf("%s\n", buf);
    }
    else {
        printf("Error! Symbolic link does not exist.");
    }
}
void catLinkContent(char *path) {
    char buf[SIZE];
    long len = readlink(path, buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        myCat(buf);
    }
    else {
        printf("Error! Symbolic link does not exist.");
    }
}
void myLn(char *destPath, char *linkPath) {
    link(destPath, linkPath);
}
void myLsl(char *path) {
    struct stat sb;
    if (stat(path, &sb) == -1) {
        printf("Error! Entity does not exist.");
    }
    else {
        printf("Permissions: %o\n", sb.st_mode & 0777);
        printf("File Permissions: \t");
        printf( (S_ISDIR(sb.st_mode)) ? "d" : "-");
        printf( (sb.st_mode & S_IRUSR) ? "r" : "-");
        printf( (sb.st_mode & S_IWUSR) ? "w" : "-");
        printf( (sb.st_mode & S_IXUSR) ? "x" : "-");
        printf( (sb.st_mode & S_IRGRP) ? "r" : "-");
        printf( (sb.st_mode & S_IWGRP) ? "w" : "-");
        printf( (sb.st_mode & S_IXGRP) ? "x" : "-");
        printf( (sb.st_mode & S_IROTH) ? "r" : "-");
        printf( (sb.st_mode & S_IWOTH) ? "w" : "-");
        printf( (sb.st_mode & S_IXOTH) ? "x" : "-");
        printf("\n\n");

        printf("Number of hard links: %ld\n", sb.st_nlink);
    }
}

  
void myChmod(char *path, mode_t mode) {
    if (chmod(path, mode) == -1) {
        printf("Error! Can't change permissions on file.");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return -1;
    }
    char *action = argv[0];
    char *path = argv[1];

    if (strcmp(action, "./mkdir") == 0) {
        if (argc != 3) {
            printf("mkdir <path> <mode>\n");
            return -1;
        }
        mode_t mode = strtol(argv[2], NULL, 8);
        myMkdir(path, mode);
    }
    else if (strcmp(action, "./ls") == 0) {
        myLs(path);
    } 
    else if (strcmp(action, "./rmdir") == 0) {
        myRmdir(path);
    }
    else if (strcmp(action, "./touch") == 0) {
        myTouch(path);
    }
    else if (strcmp(action, "./cat") == 0) {
        myCat(path);
    } 
    else if (strcmp(action, "./rm") == 0) {
        myRm(path);
    } 
    else if (strcmp(action, "./lnSym") == 0) {
        if (argc != 3) {
            printf("ln <target> <link>\n");
            return -1;
        }
        char *destPath = argv[1];
        char *linkPath = argv[2];
        mySymLn(destPath, linkPath);
    } 
    else if (strcmp(action, "./readlink") == 0) {
        myReadlink(path);
    }
    else if (strcmp(action, "./catLinkContent") == 0) {
        catLinkContent(path);
    }
    else if (strcmp(action, "./rmSymLink") == 0) {
        myRm(path);
    }
    else if (strcmp(action, "./ln") == 0) {
        if (argc < 4) {
            printf("ln <path> <mode>\n");
            return 1;
        }
        char *destPath = argv[1];
        char *linkPath = argv[2];
        myLn(destPath, linkPath);
    }
    else if (strcmp(action, "./rmHardLink") == 0) {
        myRm(path);
    } 
    else if (strcmp(action, "./lsl") == 0) {
        myLsl(path);
    }
    else if (strcmp(action, "./cmod") == 0) {
        if (argc != 2) {
            printf("cmod <target> <link>\n");
            return 1;
        }
        mode_t mode = strtol(argv[2], NULL, 8);
        myChmod(path, mode);
    } 
    else {
        printf("error");
    }
    return 0;
}