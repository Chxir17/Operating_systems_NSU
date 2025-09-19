#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

typedef struct {
    uintptr_t start;
    uintptr_t end;
    char pathname[256];
} Mapping;

void print_entry(uintptr_t addr, unsigned long long entry, const char *pathname) {
    int present = (entry >> 63) & 1;
    int swapped = (entry >> 62) & 1;
    unsigned long long  pfn = entry & ((1ULL << 55) - 1); //0-54
    printf("Virtual Address: 0x%012lx | present: %d | swapped: %d | Page Frame Nmber: 0x%-10llx | => %s\n", addr, present, swapped, pfn, pathname);
}

void read_pagemap(const char *pagemap_path, Mapping *mappings, long long mapping_count) {
    int pagemap_fd = open(pagemap_path, O_RDONLY);
    if (pagemap_fd < 0) {
        perror("Failed to open pagemap");
        exit(EXIT_FAILURE);
    }

    long page_size = sysconf(_SC_PAGESIZE);

    for (long long i = 0; i < mapping_count; i++) {
        for (uintptr_t addr = mappings[i].start; addr < mappings[i].end; addr += page_size) {
            off_t offset = (addr / page_size) * 8;

            if (lseek(pagemap_fd, offset, SEEK_SET) == (off_t)-1) {
                perror("Failed to seek pagemap");
                close(pagemap_fd);
                exit(EXIT_FAILURE);
            }

            unsigned long long entry;
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

Mapping *parse_maps(const char *maps_path, long long *mapping_count) {
    FILE *maps_file = fopen(maps_path, "r");
    if (!maps_file) {
        perror("Failed to open maps");
        exit(EXIT_FAILURE);
    }
    Mapping *mappings = malloc(sizeof(Mapping) * 1024);
    long long count = 0;

    char line[1024];
    while (fgets(line, sizeof(line), maps_file)) {
        uintptr_t start, end;
        char perms[5], dev[6], mapname[256] = "";
        unsigned long offset, inode;

        int matched = sscanf(line, "%lx-%lx %4s %lx %5s %lu %255[^\n]", &start, &end, perms, &offset, dev, &inode, mapname);

        if (matched < 6) {
            continue;
        }

        mappings[count].start = start;
        mappings[count].end = end;
        if (matched == 7) {
            strncpy(mappings[count].pathname, mapname, sizeof(mappings[count].pathname) - 1);
        }
        else{
            strcpy(mappings[count].pathname, "[unknown]");
        }
        count++;
    }

    fclose(maps_file);
    *mapping_count = count;
    return mappings;
}

int main(int argc, char *argv[]) {
    char maps_path[256];
    char pagemap_path[256];

    if (strcmp(argv[1], "self") == 0) {
        snprintf(maps_path, sizeof(maps_path), "/proc/self/maps");
        snprintf(pagemap_path, sizeof(pagemap_path), "/proc/self/pagemap");
    } else {
        snprintf(maps_path, sizeof(maps_path), "/proc/%s/maps", argv[1]);
        snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%s/pagemap", argv[1]);
    }

    long long сount;
    Mapping *mappings = parse_maps(maps_path, &сount);
    read_pagemap(pagemap_path, mappings, сount);

    free(mappings);
    return 0;
}


