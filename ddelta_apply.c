/* ddelta_apply.c - A sane reimplementation of bspatch
 * 
 * Copyright (C) 2017 Julian Andres Klode <jak@debian.org>
 */

#include <endian.h>
#include <stdint.h>
#include <stdio.h>

/* Fork of BSDIFF that does not compress ctrl, diff, extra blocks */
#define DDELTA_MAGIC "DDELTA40"

/**
 * A ddelta file has the following format:
 *
 * * the header
 * * a list of entries
 */
struct ddelta_header {
    char magic[8];
    uint64_t new_file_size;
};

/**
 * An entry consists of this header, followed by
 *
 * 1. 'diff' bytes of diff data
 * 2. 'extra' bytes of extra data
 */
struct ddelta_entry_header {
    uint64_t diff;
    uint64_t extra;
    union {
        int64_t value;
        uint64_t raw;
    } seek;
};

static int ddelta_header_read(struct ddelta_header *header, FILE *file)
{
    if (fread_unlocked(header, sizeof(*header), 1, file) < 1)
        return -1;

    header->new_file_size = be64toh(header->new_file_size);
    return 0;
}

static int ddelta_entry_header_read(struct ddelta_entry_header *entry,
                                    FILE *file)
{
    if (fread_unlocked(entry, sizeof(*entry), 1, file) < 1)
        return -1;

    entry->diff = be64toh(entry->diff);
    entry->extra = be64toh(entry->extra);
    entry->seek.raw = be64toh(entry->seek.raw);
    return 0;
}

static int copy_bytes(FILE *a, FILE *b, uint64_t bytes)
{
    char buf[4096];
    for (uint64_t i = 0; i < bytes; i++) {
        uint64_t toread = sizeof(buf);
        if (toread > bytes)
            toread = bytes;

        if (fread_unlocked(&buf, toread, 1, a) < 1)
            return -1;
        if (fwrite_unlocked(&buf, toread, 1, b) < 1)
            return -1;

        bytes -= toread;
    }
    return 0;
}

/**
 * Apply a ddelta_apply in patchfd to oldfd, writing to newfd.
 *
 * The oldfd must be seekable, the patchfd and newfd are read/written
 * sequentially.
 */
int ddelta_apply(FILE *patchfd, FILE *oldfd, FILE *newfd)
{
    struct ddelta_header header;
    struct ddelta_entry_header entry;

    if (ddelta_header_read(&header, patchfd) < 0)
        return -1;

    while (ddelta_entry_header_read(&entry, patchfd) == 0) {
        /* Apply the diff */
        for (uint64_t i = 0; i < entry.diff; i++) {
            unsigned char old;
            unsigned char patch;

            if (fread_unlocked(&patch, 1, 1, patchfd) < 1)
                return -1;
            if (fread_unlocked(&old, 1, 1, oldfd) < 1)
                return -1;

            unsigned char new = old + patch;
            if (fwrite_unlocked(&new, 1, 1, newfd) < 1)
                return -1;
        }

        // Copy the bytes over
        if (copy_bytes(patchfd, newfd, entry.extra) < 0)
            return -1;

        // Skip remaining bytes
        if (fseek(oldfd, entry.seek.value, SEEK_CUR) < 0) {
            fprintf(stderr, "Could not seek %ld bytes", entry.seek.value);
            return -1;
        }
    }

    fflush(newfd);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s oldfile newfile patchfile\n", argv[0]);
    }

    FILE *old = fopen(argv[1], "rb");
    FILE *new = fopen(argv[2], "wb");
    FILE *patch = fopen(argv[3], "rb");

    if (old == NULL)
        return perror("Cannot open old"), 1;
    if (new == NULL)
        return perror("Cannot open new"), 1;
    if (patch == NULL)
        return perror("Cannot open patch"), 1;

    printf("Result: %d\n", ddelta_apply(patch, old, new));

    return 0;
}
