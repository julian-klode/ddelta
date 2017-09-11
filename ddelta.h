#ifndef DDELTA_H
#define DDELTA_H

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

/**
 * Generates a diff from the files in oldfd and newfd in patchfd.
 *
 * The old and new files must be seekable.
 *
 * All files will be closed after the call, if you want to keep them
 * open, you must pass a dup()ed file descritopr.
 */
int ddelta_generate(const char *oldname, int oldfd, const char *newname,
                    int newfd, const char *patchname, int patchfd);

/**
 * Generates a new file from a given patch and an old file.
 *
 * The old file must be seekable.
 */
int ddelta_apply(FILE *patchfd, FILE *oldfd, FILE *newfd);

#endif
