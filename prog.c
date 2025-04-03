#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

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
        return -1;
    }
    return mkdir(path, 0777);
}

char *reverseDirName(const char *srcPath, long pathLen, long lastSlash) {
    char *reversedName = malloc((pathLen - lastSlash) * sizeof(char));
    if (!reversedName){
        return NULL;
    }
    for (long i = pathLen - 1, j = 0; i > lastSlash; i--, j++) {
        reversedName[j] = srcPath[i];
    }
    reversedName[pathLen - lastSlash - 1] = '\0';

    char *reversedPath = malloc(pathLen * sizeof(char));
    if (!reversedPath) {
        free(reversedName);
        return NULL;
    }

    strncpy(reversedPath, srcPath, lastSlash + 1);
    strcpy(reversedPath + lastSlash + 1, reversedName);
    free(reversedName);
    return reversedPath;
}

char *reverseDir(const char *srcPath) {
    const long pathLen = strlen(srcPath);
    long lastSlash = pathLen;
    for (;lastSlash > 0 && srcPath[lastSlash] != '/'; lastSlash--) {}

    char *reversedPath = reverseDirName(srcPath, pathLen, lastSlash);
    if (reversedPath == NULL) {
        return NULL;
    }
    if (createReverseDir(reversedPath) != 0) {
        free(reversedPath);
        return NULL;
    }
    printf("Directory %s was created\n", reversedPath);
    return reversedPath;
}

long fileSize(FILE *f) {
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    return size;
}

char *createReverseName(const char *fileName) {
    long fileNameLen = strlen(fileName);
    char *reverseName = calloc(fileNameLen + 1, sizeof(char));
    if (!reverseName){
        return NULL;
    }
    int dot = -1;
    for (int i = 0; i < fileNameLen; i++) {
        if (fileName[i] == '.') {
            dot = i;
            break;
        }
    }

    if (dot == -1) {
        for (long i = 0; i < fileNameLen; i++) {
            reverseName[i] = fileName[fileNameLen - 1 - i];
        }
    } 
    else {
        for (int i = 0; i < dot; i++) {
            reverseName[i] = fileName[dot - 1 - i];
        }
        strcpy(reverseName + dot, fileName + dot);
    }
    return reverseName;
}

void copyContent(FILE *source, FILE *destination) {
    long size = fileSize(source);
    if (size == -1){
        return;
    }
    fseek(source, -1, SEEK_END); //переход к концу файла 
    for (long i = 0; i < size; i++) {
        fputc(fgetc(source), destination);
        fseek(source, -2, SEEK_CUR);
    }
}

char *getPath(const char *path, const char *name) {
    long len = strlen(path) + strlen(name) + 2;
    char *full = calloc(len, sizeof(char));
    if (!full){
        return NULL;
    }
    snprintf(full, len, "%s/%s", path, name);
    return full;
}

int createReverseFile(const char *srcPath, const char *srcName, const char *reversedPath, const char *reversedName) {
    char *source = getPath(srcPath, srcName);
    char *destination = getPath(reversedPath, reversedName);

    FILE *in = fopen(source, "r");
    if (!in){
        return 0;
    }
    FILE *out = fopen(destination, "w");
    if (!out) {
        fclose(in);
        return 0;
    }
    copyContent(in, out);
    fclose(in);
    fclose(out);
    free(source);
    free(destination);
    return 1;
}

int reverseFiles(const char *srcPath, const char *reversedPath) {
    DIR *dir = opendir(srcPath);
    if (!dir){
        return 0;
    }
    struct dirent *entry; // структура для хранения информации о файлах/папках при обходе.
    struct stat entryStat; //структура для получения информации о файле.

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }
        char *fullSrcPath = getPath(srcPath, entry->d_name);
        stat(fullSrcPath, &entryStat);

        if (S_ISREG(entryStat.st_mode)) {
            char *reversedFileName = createReverseName(entry->d_name);
            if (!createReverseFile(srcPath, entry->d_name, reversedPath, reversedFileName)) {
                free(fullSrcPath);
                free(reversedFileName);
                closedir(dir);
                return 0;
            }
            free(reversedFileName);
        } 
        else if (S_ISDIR(entryStat.st_mode)) {
            char *reversedDirName = createReverseName(entry->d_name);
            char *newDestPath = getPath(reversedPath, reversedDirName);
            free(reversedDirName);
            if (!newDestPath) {
                free(fullSrcPath);
                closedir(dir);
                return 0;
            }
            if (createReverseDir(newDestPath) != 0) {
                free(fullSrcPath);
                free(newDestPath);
                closedir(dir);
                return 0;
            }
            if (!reverseFiles(fullSrcPath, newDestPath)) {
                free(fullSrcPath);
                free(newDestPath);
                closedir(dir);
                return 0;
            }
            free(newDestPath);
        }
        free(fullSrcPath);
    }
    closedir(dir);
    return 1;
}



int main(int argc, char **argv) {
    if (argc != 2){
        printf("Enter only directory path as an program argument");
        return -1;
    }
    char *srcPath = argv[1];
    char *reversePath = reverseDir(srcPath);
    
    if (!reversePath){
        printf("Directory wasn't reverse");
        return -1;
    }
    if (!reverseFiles(srcPath, reversePath)) {
        printf("Files weren't reverse");
        free(reversePath);
        return -1;
    }

    free(reversePath);
    printf("Done\n");
    return 0;
}
