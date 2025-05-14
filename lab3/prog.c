#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#define PATH_MAX 4096
#define BUF_SIZE 1024

enum ERRORS {
    SUCCESS = 1,
    ALREADY_EXIST = 0,
    FSEEK_ERROR = 2,
    ARGS_ERROR = 3,
    MEMORY_ERROR = 4,
    FILE_READ_ERROR = 5,
    FILE_WRITE_ERROR = 6,
    DIR_OPEN_ERROR = 7,
    DIR_CREATE_ERROR = 8,
    FILE_OPEN_ERROR = 9,
    REVERSE_ERROR = 10,
    PATH_ERROR = 11,
    UNKNOWN_ERROR = 12
};


int checkExist(const char *reversedPath) {
    DIR *dir = opendir(reversedPath);
    int exist = 0;
    if (dir) {
        exist = 1;
        closedir(dir);
    }
    return exist;
}

int createReverseDir(const char *path) {
    if (checkExist(path)) {
        return ALREADY_EXIST;
    }
    return mkdir(path, 0777);
}

void reverseDirName(const char *srcPath, char *reversedPath, const long pathLen, const  long lastSlash) {
    char reversedName[pathLen - lastSlash];
    for (long i = pathLen - 1, j = 0; i > lastSlash; i--, j++) {
        reversedName[j] = srcPath[i];
    }
    reversedName[pathLen - lastSlash - 1] = '\0';
    strncpy(reversedPath, srcPath, lastSlash + 1);
    strcpy(reversedPath + lastSlash + 1, reversedName);
}

char *reverseDir(const char *srcPath) {
    const long pathLen = strlen(srcPath);
    long lastSlash = pathLen;
    for (;lastSlash > 0 && srcPath[lastSlash] != '/'; lastSlash--) {}
    char *reversedPath = malloc((pathLen + 1) * sizeof(char));
    if (reversedPath == NULL) {
        printf("Memory wasn't allocated\n");
        return NULL;
    }
    reverseDirName(srcPath, reversedPath, pathLen, lastSlash);
    if (createReverseDir(reversedPath) != 0) {
        printf("Directory with reversed name already exist\n");
        free(reversedPath);
        return NULL;
    }
    printf("Directory %s was created\n", reversedPath);
    return reversedPath;
}

long fileSize(FILE *f) {
    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek SEEK_END");
        return FSEEK_ERROR;
    }
    const long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) {
        perror("fseek SEEK_SET");
        return FSEEK_ERROR;
    }
    return size;
}

void createReverseName(const char *fileName, char *reverseName) {
    const long fileNameLen = strlen(fileName);
    for (long i = 0; i < fileNameLen; i++) {
        reverseName[i] = fileName[fileNameLen - 1 - i];
    }
}


void copyContent(FILE *source, FILE *destination) {
    long size = fileSize(source);
    if (size == FSEEK_ERROR) {
        perror("File size not received");
        return;
    }
    char buffer[BUF_SIZE];
    for (long remaining = size; remaining > 0;) {
        long long toRead = remaining >= BUF_SIZE ? BUF_SIZE : remaining;
        long pos = remaining - toRead;

        if (fseek(source, pos, SEEK_SET) != 0) {
            perror("Can't seek in source file");
            return;
        }

        if (fread(buffer, 1, toRead, source) != toRead) {
            perror("Error reading source file");
            return;
        }
        for (long long i = 0; i < toRead / 2; i++) {
            char tmp = buffer[i];
            buffer[i] = buffer[toRead - i - 1];
            buffer[toRead - i - 1] = tmp;
        }

        if (fwrite(buffer, 1, toRead, destination) != toRead) {
            perror("Error writing to destination file");
            return;
        }
        remaining -= toRead;
    }
}


void getPath(const char *path, const char *name, char *full, long len) {
    snprintf(full, len, "%s/%s", path, name);
}

int createReverseFile(const char *srcPath, const char *srcName, const char *reversedPath, const char *reversedName) {
    long srcLen = strlen(srcPath) + strlen(srcName) + 2;
    char source[srcLen];
    getPath(srcPath, srcName, source, srcLen);
    long destLen = strlen(reversedPath) + strlen(reversedName) + 2;
    char destination[destLen];
    getPath(reversedPath, reversedName, destination, destLen);

    FILE *in = fopen(source, "r");
    if (!in) {
        fprintf(stderr, "Error opening file for reading: %s\n", source);
        return FILE_OPEN_ERROR;
    }
    FILE *out = fopen(destination, "w+");
    if (!out) {
        fprintf(stderr, "Error opening file for writing: %s\n", destination);
        fclose(in);
        return FILE_OPEN_ERROR;
    }
    copyContent(in, out);
    fclose(in);
    fclose(out);
    return SUCCESS;
}

int reverseFiles(const char *srcPath, const char *reversedPath) {
    DIR *dir = opendir(srcPath);
    if (!dir) {
        fprintf(stderr, "Failed to open directory: %s\n", srcPath);
        return DIR_OPEN_ERROR;
    }

    struct dirent *entry;
    struct stat entryStat;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        long len = strlen(srcPath) + strlen(entry->d_name) + 2;
        char fullSrcPath[len];
        getPath(srcPath, entry->d_name, fullSrcPath, len);

        if (stat(fullSrcPath, &entryStat) != 0) {
            fprintf(stderr, "Cannot stat file: %s\n", fullSrcPath);
            continue;
        }

        if (S_ISREG(entryStat.st_mode)) {
            const long fileNameLen = strlen(entry->d_name);
            char reversedFileName[fileNameLen + 1];
            reversedFileName[fileNameLen] = '\0';
            createReverseName(entry->d_name, reversedFileName);
            if (createReverseFile(srcPath, entry->d_name, reversedPath, reversedFileName) != SUCCESS) {
                fprintf(stderr, "Failed to reverse file: %s\n", fullSrcPath);
                continue;
            }
        }
        else if (S_ISDIR(entryStat.st_mode)) {
            const long DirNameLen = strlen(entry->d_name);
            char reversedDirName[DirNameLen + 1];
            reversedDirName[DirNameLen] = '\0';
            createReverseName(entry->d_name, reversedDirName);

            char newDestPath[len];
            getPath(reversedPath, reversedDirName, newDestPath, len);

            if (createReverseDir(newDestPath) != 0) {
                fprintf(stderr, "Failed to create subdirectory: %s\n", newDestPath);
                continue;
            }

            if (reverseFiles(fullSrcPath, newDestPath) != SUCCESS) {
                fprintf(stderr, "Failed to reverse subdirectory: %s\n", fullSrcPath);
                continue;
            }
        }
    }

    closedir(dir);
    return SUCCESS;
}



int main(int argc, char **argv) {
    if (argc != 2){
        fprintf(stderr, "Enter only directory path as a program argument\n");
        return ARGS_ERROR;
    }

    char absSrcPath[PATH_MAX];
    if (!realpath(argv[1], absSrcPath)) {
        perror("Error with getting full path");
        return PATH_ERROR;
    }

    char *reversePath = reverseDir(absSrcPath);
    if (!reversePath){
        fprintf(stderr, "Directory wasn't reversed\n");
        return REVERSE_ERROR;
    }

    if (reverseFiles(absSrcPath, reversePath) != SUCCESS) {
        fprintf(stderr, "Files weren't reversed\n");
        free(reversePath);
        return REVERSE_ERROR;
    }

    free(reversePath);
    printf("Done\n");
    return SUCCESS;
}
