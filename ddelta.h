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
 * Error codes to be returned by ddelta functions.
 *
 * Each function returns a negated variant of these error code on success, and for the
 * I/O errors, more information is available in errno.
 */
enum ddelta_error {
    /** The patch file has an invalid magic or header could not be read */
    DDELTA_EMAGIC = 1,
    /** An unknown algorithm error occured */
    DDELTA_EALGO,
    /** An I/O error occured while reading from (apply) or writing to (generate) the patch file */
    DDELTA_EPATCHIO,
    /** An I/O error occured while reading from the old file */
    DDELTA_EOLDIO,
    /** An I/O error occured while reading from (generate) or writing to (apply) the new file */
    DDELTA_ENEWIO
};

/**
 * Generates a diff from the files in oldfd and newfd in patchfd.
 *
 * The old and new files must be seekable.
 *
 * All files will be closed after the call, if you want to keep them
 * open, you must pass a dup()ed file descritopr.
 */
int ddelta_generate(int oldfd, int newfd, int patchfd);

/**
 * Generates a new file from a given patch and an old file.
 *
 * The old file must be seekable.
 */
int ddelta_apply(FILE *patchfd, FILE *oldfd, FILE *newfd);

#endif
