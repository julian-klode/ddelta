#ifndef DDELTA_H
#define DDELTA_H

#include <stdio.h>

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
