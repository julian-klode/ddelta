#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define MIN(A, B) (((A) < (B)) ? (A) : (B))

struct buffered_fd {
    int fd;
    size_t bufstart;
    size_t bufused;
    char buf[4 * 1024];
};

static ssize_t safewrite(int fd, void *buf, size_t count)
{
    size_t written = 0;

    for (written = 0; written < count;) {
        ssize_t this_write = write(fd, buf + written, count - written);

        if (this_write < 0)
            return this_write;

        written += this_write;
    }
    return written;
}

static ssize_t saferead(int fd, void *buf, size_t count)
{
    size_t bytes_read = 0;

    for (bytes_read = 0; bytes_read < count;) {
        ssize_t this_read = read(fd, buf + bytes_read, count - bytes_read);

        if (this_read < 0)
            return this_read;
        if (this_read == 0)
            break;

        bytes_read += this_read;
    }
    return bytes_read;
}

/** Returns 0 on success, -1 on failure */
static int buffered_fd_flush(struct buffered_fd *bfd)
{
    //fprintf(stderr, "flushing %d\n", bfd->fd);
    if (safewrite(bfd->fd, bfd->buf, bfd->bufused) != bfd->bufused)
        return -1;

    bfd->bufused = 0;
    return 0;
}

/** Returns 0 on success, -1 on failure */
static ssize_t buffered_fd_write(struct buffered_fd *bfd, void *buf,
                                 size_t size)
{
    const size_t full_size = size;

    // Optimization: Directly write from buffer to fd
    if (0 && 0 == bfd->bufused && size >= sizeof(bfd->buf))
        return safewrite(bfd->fd, buf, size);

    while (size > 0) {
        if (bfd->bufused < sizeof(bfd->buf)) {
            size_t tofill = MIN(size, sizeof(bfd->buf) - bfd->bufused);
            memcpy(bfd->buf + bfd->bufused, buf, tofill);
            size -= tofill;
            buf += tofill;
            bfd->bufused += tofill;
        }

        if (bfd->bufused == sizeof(bfd->buf) && buffered_fd_flush(bfd) < 0)
            return -1;
    }

    return full_size;
}

/** Returns read bytes on success, -1 on failure */
static ssize_t buffered_fd_read(struct buffered_fd *bfd, void *buf, size_t size)
{
    size_t rd = 0;

    for (rd = 0; rd < size;) {
        // Buffer is used up, fill again
        if (bfd->bufstart == bfd->bufused) {
            bfd->bufstart = bfd->bufused = 0;
            // Optimize case where we read value larger than buffer
            if (size - rd >= sizeof(bfd->buf)) {
                ssize_t rd2 = saferead(bfd->fd, buf + rd, size - rd);
                return (rd2 < 0) ? rd2 : rd2 + rd;
            }

            ssize_t bufused = saferead(bfd->fd, bfd->buf, sizeof(bfd->buf));

            if (bufused == 0)
                break;

            if (bufused < 0)
                return bufused;

            bfd->bufused = bufused;
            bfd->bufstart = 0;
        }

        size_t tocopy = MIN(size - rd, bfd->bufused - bfd->bufstart);
        memcpy(buf + rd, bfd->buf + bfd->bufstart, tocopy);
        rd += tocopy;
        bfd->bufstart += tocopy;
    }

    return rd;
}

static off_t buffered_fd_seek(struct buffered_fd *bfd, off_t offset, int whence)
{
    if ((off_t) bfd->bufstart + offset >= 0
        && bfd->bufstart + offset < bfd->bufused) {
        bfd->bufstart += offset;
        return 0;
    }

    off_t local_offset = whence == SEEK_CUR ? bfd->bufused - bfd->bufstart : 0;
    bfd->bufused = 0;
    bfd->bufstart = 0;
    return lseek(bfd->fd, offset - local_offset, whence);
}
