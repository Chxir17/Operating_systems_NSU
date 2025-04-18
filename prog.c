#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#define PATH_MAX 4096

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
        return FSEEK_ERROR;
    }
    const long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) {
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
        printf("File size not received\n");
        return;
    }

    char *buffer = malloc(size);
    if (buffer == NULL) {
        printf("Memory allocation error\n");
        return;
    }
    if (fseek(source, 0, SEEK_SET) != 0) {
        printf("Can't set position in source file");
        free(buffer);
        return;
    }

    if (fread(buffer, 1, size, source) != size) {
        printf("Error reading source file\n");
        free(buffer);
        return;
    }

    for (long i = 0; i < size / 2; i++) {
        char temp = buffer[i];
        buffer[i] = buffer[size - i - 1];
        buffer[size - i - 1] = temp;
    }

    if (fseek(destination, 0, SEEK_SET) != 0) {
        printf("Can't set position in destination file\n");
        free(buffer);
        return;
    }

    if (fwrite(buffer, 1, size, destination) != size) {
        printf("Error writing to destination file\n");
    }
    free(buffer);
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
    if (!in){
        return FILE_OPEN_ERROR;
    }
    FILE *out = fopen(destination, "w+");
    if (!out) {
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
    if (!dir){
        printf("Failed to open source directory");
        return DIR_OPEN_ERROR;
    }
    struct dirent *entry;
    struct stat entryStat;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }
        long len = strlen(srcPath) + strlen(entry->d_name) + 2;
        char fullSrcPath[len];
        getPath(srcPath, entry->d_name, fullSrcPath, len);
        stat(fullSrcPath, &entryStat);

        if (S_ISREG(entryStat.st_mode)) {
            const long fileNameLen = strlen(entry->d_name);
            char reversedFileName[fileNameLen];
            createReverseName(entry->d_name, reversedFileName);
            if (createReverseFile(srcPath, entry->d_name, reversedPath, reversedFileName) != 1) {
                closedir(dir);
                return REVERSE_ERROR;
            }
        } 
        else if (S_ISDIR(entryStat.st_mode)) {
            const long DirNameLen = strlen(entry->d_name);
            char reversedDirName[DirNameLen];
            createReverseName(entry->d_name, reversedDirName);
            char newDestPath[len];
            getPath(reversedPath, reversedDirName, newDestPath, len);
            if (createReverseDir(newDestPath) != 0) {
                closedir(dir);
                return DIR_CREATE_ERROR;
            }
            if (!reverseFiles(fullSrcPath, newDestPath)) {
                closedir(dir);
                return REVERSE_ERROR;
            }
        }
    }
    closedir(dir);
    return SUCCESS;
}



int main(int argc, char **argv) {
    if (argc != 2){
        printf("Enter only directory path as a program argument\n");
        return ARGS_ERROR;
    }

    char absSrcPath[PATH_MAX];
    if (!realpath(argv[1], absSrcPath)) {
        printf("Error with getting full path\n");
        return PATH_ERROR;
    }

    char *reversePath = reverseDir(absSrcPath);
    if (!reversePath){
        printf("Directory wasn't reversed\n");
        return REVERSE_ERROR;
    }

    if (reverseFiles(absSrcPath, reversePath) != 1) {
        printf("Files weren't reversed\n");
        free(reversePath);
        return REVERSE_ERROR;
    }

    free(reversePath);
    printf("Done\n");
    return SUCCESS;
}
