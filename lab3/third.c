#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#define PAGEMAP_ENTRY_SIZE 8

void read_pagemap(const char *pagemap_path, uintptr_t start, uintptr_t end) {
    int pagemap_fd = open(pagemap_path, O_RDONLY);
    if (pagemap_fd < 0) {
        perror("Failed to open pagemap");
        exit(EXIT_FAILURE);
    }

    for (uintptr_t addr = start; addr < end; addr += sysconf(_SC_PAGESIZE)) {
        off_t offset = (addr / sysconf(_SC_PAGESIZE)) * PAGEMAP_ENTRY_SIZE;

        if (lseek(pagemap_fd, offset, SEEK_SET) == (off_t)-1) {
            perror("Failed to seek pagemap");
            close(pagemap_fd);
            exit(EXIT_FAILURE);
        }

        uint64_t entry;
        if (read(pagemap_fd, &entry, sizeof(entry)) != sizeof(entry)) {
            perror("Failed to read pagemap");
            close(pagemap_fd);
            exit(EXIT_FAILURE);
        }

        printf("Address: 0x%lx, Pagemap Entry: 0x%llx\n", addr, (unsigned long long)entry);
    }

    close(pagemap_fd);
}

void parse_maps_and_read_pagemap(const char *maps_path, const char *pagemap_path) {
    FILE *maps_file = fopen(maps_path, "r");
    if (!maps_file) {
        perror("Failed to open maps");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), maps_file)) {
        uintptr_t start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) != 2) {
            fprintf(stderr, "Failed to parse line: %s\n", line);
            continue;
        }

        printf("Segment: 0x%lx-0x%lx\n", start, end);
        read_pagemap(pagemap_path, start, end);
    }

    fclose(maps_file);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <self|pid>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char maps_path[256];
    char pagemap_path[256];

    if (strcmp(argv[1], "self") == 0) {
        snprintf(maps_path, sizeof(maps_path), "/proc/self/maps");
        snprintf(pagemap_path, sizeof(pagemap_path), "/proc/self/pagemap");
    }
    else {
        snprintf(maps_path, sizeof(maps_path), "/proc/%s/maps", argv[1]);
        snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%s/pagemap", argv[1]);
    }
    parse_maps_and_read_pagemap(maps_path, pagemap_path);
    return 0;
}