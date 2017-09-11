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

#include "ddelta.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

/* Size of blocks to work on at once */
#ifndef DDELTA_BLOCK_SIZE
#define DDELTA_BLOCK_SIZE (32 * 1024)
#endif

static uint64_t ddelta_be64toh(uint64_t be64)
{
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap64(be64);
#elif defined(__GNUC__) && defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return be64;
#else
    unsigned char *buf = (unsigned char *) &be64;

    return (uint64_t) buf[0] << 56 |
           (uint64_t) buf[1] << 48 |
           (uint64_t) buf[2] << 40 |
           (uint64_t) buf[3] << 32 |
           (uint64_t) buf[4] << 24 |
           (uint64_t) buf[5] << 16 |
           (uint64_t) buf[6] << 8 |
           (uint64_t) buf[7] << 0;
#endif
}

static int64_t ddelta_from_unsigned(uint64_t u)
{
    return u & 0x80000000 ? -(int64_t) ~(u - 1) : (int64_t) u;
}

static int ddelta_header_read(struct ddelta_header *header, FILE *file)
{
    if (fread(header, sizeof(*header), 1, file) < 1)
        return -1;

    header->new_file_size = ddelta_be64toh(header->new_file_size);
    return 0;
}

static int ddelta_entry_header_read(struct ddelta_entry_header *entry,
                                    FILE *file)
{
    if (fread(entry, sizeof(*entry), 1, file) < 1)
        return -1;

    entry->diff = ddelta_be64toh(entry->diff);
    entry->extra = ddelta_be64toh(entry->extra);
    entry->seek.value = ddelta_from_unsigned(ddelta_be64toh(entry->seek.raw));
    return 0;
}

static int apply_diff(FILE *patchfd, FILE *oldfd, FILE *newfd, uint64_t size)
{
#ifdef __GNUC__
    typedef unsigned char uchar_vector __attribute__((vector_size(16)));
#else
    typedef unsigned char uchar_vector;
#endif
    uchar_vector old[DDELTA_BLOCK_SIZE / sizeof(uchar_vector)];
    uchar_vector patch[DDELTA_BLOCK_SIZE / sizeof(uchar_vector)];

    /* Apply the diff */
    while (size > 0) {
        unsigned int i;
        const uint64_t toread = MIN(sizeof(old), size);
        const uint64_t items_to_add = MIN(sizeof(uchar_vector) + toread,
                                          sizeof(old)) /
                                      sizeof(uchar_vector);

        if (fread(&patch, 1, toread, patchfd) < toread)
            return -1;
        if (fread(&old, 1, toread, oldfd) < toread)
            return -1;

        for (i = 0; i < items_to_add; i++)
            old[i] += patch[i];

        if (fwrite(&old, 1, toread, newfd) < toread)
            return -1;

        size -= toread;
    }

    return 0;
}

static int copy_bytes(FILE *a, FILE *b, uint64_t bytes)
{
    char buf[DDELTA_BLOCK_SIZE];
    while (bytes > 0) {
        uint64_t toread = MIN(sizeof(buf), bytes);

        if (fread(&buf, toread, 1, a) < 1)
            return -1;
        if (fwrite(&buf, toread, 1, b) < 1)
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

    if (memcmp(DDELTA_MAGIC, header.magic, sizeof(header.magic)) != 0)
        return -42;

    while (ddelta_entry_header_read(&entry, patchfd) == 0) {
        if (entry.diff == 0 && entry.extra == 0 && entry.seek.value == 0) {
            fflush(newfd);
            return 0;
        }

        if (apply_diff(patchfd, oldfd, newfd, entry.diff) < 0)
            return -1;

        /* Copy the bytes over */
        if (copy_bytes(patchfd, newfd, entry.extra) < 0)
            return -1;

        /* Skip remaining bytes */
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
    FILE *old;
    FILE *new;
    FILE *patch;

    if (argc != 4) {
        fprintf(stderr, "usage: %s oldfile newfile patchfile\n", argv[0]);
    }

    old = fopen(argv[1], "rb");
    new = fopen(argv[2], "wb");
    patch = fopen(argv[3], "rb");

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
