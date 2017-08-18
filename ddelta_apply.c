/* ddelta_apply.c - A sane reimplementation of bspatch
 * 
 * Copyright (C) 2017 Julian Andres Klode <jak@debian.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

static int apply_diff(FILE *patchfd, FILE *oldfd, FILE *newfd, uint64_t size)
{
#define DIFF_BUFFER_SIZE 512
    /* Apply the diff */
    while (size > 0) {
        unsigned char old[DIFF_BUFFER_SIZE];
        unsigned char patch[DIFF_BUFFER_SIZE];
        const uint64_t toread =
            size > DIFF_BUFFER_SIZE ? DIFF_BUFFER_SIZE : size;

        if (fread_unlocked(&patch, 1, toread, patchfd) < toread)
            return -1;
        if (fread_unlocked(&old, 1, toread, oldfd) < toread)
            return -1;

        for (uint64_t j = 0; j < toread; j++) {
            old[j] += patch[j];
        }

        if (fwrite_unlocked(&old, 1, toread, newfd) < toread)
            return -1;

        size -= toread;
    }

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
        if (entry.diff == 0 && entry.extra == 0 && entry.seek.value == 0) {
            fflush(newfd);
            return 0;
        }

        if (apply_diff(patchfd, oldfd, newfd, entry.diff) < 0)
            return -1;

        // Copy the bytes over
        if (copy_bytes(patchfd, newfd, entry.extra) < 0)
            return -1;

        // Skip remaining bytes
        if (fseek(oldfd, entry.seek.value, SEEK_CUR) < 0) {
            fprintf(stderr, "Could not seek %ld bytes", entry.seek.value);
            return -1;
        }
    }

    return -1;
}

#ifndef DDELTA_NO_MAIN
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
#endif
