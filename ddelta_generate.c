/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2017 Julian Andres Klode <jak@jak-linux.org>
 * All rights reserved
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

#include <sys/types.h>

#include <endian.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <divsufsort.h>

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static off_t matchlen(u_char * old, off_t oldsize, u_char * new, off_t newsize)
{
    off_t i;

    for (i = 0; (i < oldsize) && (i < newsize); i++)
        if (old[i] != new[i])
            break;

    return i;
}

static off_t search(saidx_t * I, u_char * old, off_t oldsize,
                    u_char * new, off_t newsize, off_t st, off_t en,
                    off_t * pos)
{
    off_t x, y;

    if (en - st < 2) {
        x = matchlen(old + I[st], oldsize - I[st], new, newsize);
        y = matchlen(old + I[en], oldsize - I[en], new, newsize);

        if (x > y) {
            *pos = I[st];
            return x;
        } else {
            *pos = I[en];
            return y;
        }
    };

    x = st + (en - st) / 2;
    if (memcmp(old + I[x], new, MIN(oldsize - I[x], newsize)) <= 0) {
        return search(I, old, oldsize, new, newsize, x, en, pos);
    } else {
        return search(I, old, oldsize, new, newsize, st, x, pos);
    };
}

void write64(FILE *file, int64_t off)
{
    union {
        int64_t i;
        uint64_t u;
    } u;

    u.i = off;

    u.u = htobe64(u.u);
    if (fwrite(&u, sizeof(u), 1, file) < 1)
        err(1, "fwrite(output)");
}

int main(int argc, char *argv[])
{
    int fd;
    u_char *old, *new;
    off_t oldsize, newsize;
    saidx_t *I;
    off_t scan, pos = 0, len;
    off_t lastscan, lastpos, lastoffset;
    off_t oldscore, scsc;
    off_t s, Sf, lenf, Sb, lenb;
    off_t overlap, Ss, lens;
    off_t i;
    FILE *pf;

    if (argc != 4)
        errx(1, "usage: %s oldfile newfile patchfile\n", argv[0]);

    /* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
       that we never try to malloc(0) and get a NULL pointer */
    if (((fd = open(argv[1], O_RDONLY, 0)) < 0) ||
        ((oldsize = lseek(fd, 0, SEEK_END)) == -1) ||
        ((old = malloc(oldsize + 1)) == NULL) ||
        (lseek(fd, 0, SEEK_SET) != 0) ||
        (read(fd, old, oldsize) != oldsize) || (close(fd) == -1))
        err(1, "%s", argv[1]);

    if (oldsize > INT32_MAX) {
        err(1, "File to large: %s", argv[1]);
    }

    if (((I = malloc((oldsize + 1) * sizeof(saidx_t))) == NULL))
        err(1, NULL);

    if (divsufsort(old, I, oldsize))
        err(1, "divsufsort");

    /* Allocate newsize+1 bytes instead of newsize bytes to ensure
       that we never try to malloc(0) and get a NULL pointer */
    if (((fd = open(argv[2], O_RDONLY, 0)) < 0) ||
        ((newsize = lseek(fd, 0, SEEK_END)) == -1) ||
        ((new = malloc(newsize + 1)) == NULL) ||
        (lseek(fd, 0, SEEK_SET) != 0) ||
        (read(fd, new, newsize) != newsize) || (close(fd) == -1))
        err(1, "%s", argv[2]);

    if (newsize > INT32_MAX) {
        err(1, "File to large: %s", argv[2]);
    }

    /* Create the patch file */
    if ((pf = fopen(argv[3], "w")) == NULL)
        err(1, "%s", argv[3]);

    /* Header is "DDELTA40", followed by size of new file. Afterwards, there
     * are entries (see loop) */
    fputs("DDELTA40", pf);
    write64(pf, newsize);

    scan = 0;
    len = 0;
    lastscan = 0;
    lastpos = 0;
    lastoffset = 0;
    while (scan < newsize) {
        oldscore = 0;

        /* If we come across a large block of data that only differs
         * by less than 8 bytes, this loop will take a long time to
         * go past that block of data. We need to track the number of
         * times we're stuck in the block and break out of it. */
        int num_less_than_eight = 0;
        off_t prev_len, prev_oldscore, prev_pos;
        for (scsc = scan += len; scan < newsize; scan++) {
            prev_len = len;
            prev_oldscore = oldscore;
            prev_pos = pos;

            len = search(I, old, oldsize, new + scan, newsize - scan,
                         0, oldsize, &pos);

            for (; scsc < scan + len; scsc++)
                if ((scsc + lastoffset < oldsize) &&
                    (old[scsc + lastoffset] == new[scsc]))
                    oldscore++;

            if (((len == oldscore) && (len != 0)) || (len > oldscore + 8))
                break;

            if ((scan + lastoffset < oldsize) &&
                (old[scan + lastoffset] == new[scan]))
                oldscore--;

            const off_t fuzz = 8;
            if (prev_len - fuzz <= len && len <= prev_len &&
                prev_oldscore - fuzz <= oldscore &&
                oldscore <= prev_oldscore &&
                prev_pos <= pos && pos <= prev_pos + fuzz &&
                oldscore <= len && len <= oldscore + fuzz)
                ++num_less_than_eight;
            else
                num_less_than_eight = 0;
            if (num_less_than_eight > 100)
                break;
        };

        if ((len != oldscore) || (scan == newsize)) {
            s = 0;
            Sf = 0;
            lenf = 0;
            for (i = 0; (lastscan + i < scan) && (lastpos + i < oldsize);) {
                if (old[lastpos + i] == new[lastscan + i])
                    s++;
                i++;
                if (s * 2 - i > Sf * 2 - lenf) {
                    Sf = s;
                    lenf = i;
                };
            };

            lenb = 0;
            if (scan < newsize) {
                s = 0;
                Sb = 0;
                for (i = 1; (scan >= lastscan + i) && (pos >= i); i++) {
                    if (old[pos - i] == new[scan - i])
                        s++;
                    if (s * 2 - i > Sb * 2 - lenb) {
                        Sb = s;
                        lenb = i;
                    };
                };
            };

            if (lastscan + lenf > scan - lenb) {
                overlap = (lastscan + lenf) - (scan - lenb);
                s = 0;
                Ss = 0;
                lens = 0;
                for (i = 0; i < overlap; i++) {
                    if (new[lastscan + lenf - overlap + i] ==
                        old[lastpos + lenf - overlap + i])
                        s++;
                    if (new[scan - lenb + i] == old[pos - lenb + i])
                        s--;
                    if (s > Ss) {
                        Ss = s;
                        lens = i + 1;
                    };
                };

                lenf += lens - overlap;
                lenb -= lens;
            };

            write64(pf, lenf);
            write64(pf, (scan - lenb) - (lastscan + lenf));
            write64(pf, (pos - lenb) - (lastpos + lenf));

            for (i = 0; i < lenf; i++)
                if (fputc_unlocked(new[lastscan + i] - old[lastpos + i], pf) ==
                    EOF)
                    err(1, "fputc(difference)");
            for (i = 0; i < (scan - lenb) - (lastscan + lenf); i++)
                if (fputc_unlocked(new[lastscan + lenf + i], pf) == EOF)
                    err(1, "fputc(extra)");

            lastscan = scan - lenb;
            lastpos = pos - lenb;
            lastoffset = pos - scan;
        };
    };

    // File terminator
    write64(pf, 0);
    write64(pf, 0);
    write64(pf, 0);

    if (fclose(pf))
        err(1, "fclose");

    /* Free the memory we used */
    free(I);
    free(old);
    free(new);

    return 0;
}
