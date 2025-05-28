#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#define PAGEMAP_ENTRY_SIZE 8

typedef struct {
    uintptr_t start;
    uintptr_t end;
    char pathname[256];
} Mapping;

void print_entry(uintptr_t addr, uint64_t entry, const char *pathname) {
    int present = (entry >> 63) & 1;
    int swapped = (entry >> 62) & 1;
    uint64_t pfn = entry & ((1ULL << 55) - 1);

    printf("VA: 0x%012lx | present: %d | swapped: %d | PFN: 0x%-10llx | => %s\n",
           addr, present, swapped, (unsigned long long)pfn, pathname);
}

void read_pagemap(const char *pagemap_path, Mapping *mappings, size_t mapping_count) {
    int pagemap_fd = open(pagemap_path, O_RDONLY);
    if (pagemap_fd < 0) {
        perror("Failed to open pagemap");
        exit(EXIT_FAILURE);
    }

    long page_size = sysconf(_SC_PAGESIZE);

    for (size_t i = 0; i < mapping_count; i++) {
        for (uintptr_t addr = mappings[i].start; addr < mappings[i].end; addr += page_size) {
            off_t offset = (addr / page_size) * PAGEMAP_ENTRY_SIZE;

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

            print_entry(addr, entry, mappings[i].pathname);
        }
    }

    close(pagemap_fd);
}

Mapping *parse_maps(const char *maps_path, size_t *mapping_count) {
    FILE *maps_file = fopen(maps_path, "r");
    if (!maps_file) {
        perror("Failed to open maps");
        exit(EXIT_FAILURE);
    }

    Mapping *mappings = malloc(sizeof(Mapping) * 1024); // Предположим не более 1024 сегментов
    size_t count = 0;

    char line[1024];
    while (fgets(line, sizeof(line), maps_file)) {
        uintptr_t start, end;
        char perms[5], dev[6], mapname[256] = "";
        unsigned long offset, inode;

        int matched = sscanf(line, "%lx-%lx %4s %lx %5s %lu %255[^\n]",
                             &start, &end, perms, &offset, dev, &inode, mapname);

        if (matched < 6) {
            continue;
        }

        mappings[count].start = start;
        mappings[count].end = end;
        if (matched == 7)
            strncpy(mappings[count].pathname, mapname, sizeof(mappings[count].pathname) - 1);
        else
            strcpy(mappings[count].pathname, "[anonymous]");
        count++;
    }

    fclose(maps_file);
    *mapping_count = count;
    return mappings;
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
    } else {
        snprintf(maps_path, sizeof(maps_path), "/proc/%s/maps", argv[1]);
        snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%s/pagemap", argv[1]);
    }

    size_t mapping_count;
    Mapping *mappings = parse_maps(maps_path, &mapping_count);
    read_pagemap(pagemap_path, mappings, mapping_count);

    free(mappings);
    return 0;
}
