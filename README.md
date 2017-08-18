# A more efficient fork of bsdiff
bsdiff is one of the most space-efficient binary diff implementations
available. The implementation, however, leaves a lot to be desired: Files
are read into memory completely, and space requirements are higher than they
have to be.

ddelta uses the same underlying algorithms as bsdiff, with the exception of
replacing qsufsort with divsufsort, and a new format for patch files

## Requirements

Given an old file of `m` bytes and a new file of `n` bytes.

For diffing:

* memory requirement is `5m + n` bytes (rather than `9m + n` on 64-bit systems)
* both files must be seek()able (for now)

For patching:

* memory requirement is constant (rather than `m + n`) - three buffers essentially.
* only the patch file must be seek()able

Furthermore, libdivsufsort is needed for compiling and running the diff
algorithm. It's not needed for patching.

## New patch file format

### bsdiff patch format
The bsdiff format consists of a header

    char magic[8];
    uint64_t control_size;
    uint64_t diff_size;
    uint64_t new_file_size;

and three blocks:
    unsigned char control[control_size];
    unsigned char diff[diff_size];
    unsigned char extra[];

each individually compressed with bzip2. When applying a patch, bspatch keeps
three bz2 file instances, one to each region, and steps through them.

Each control entry is a triplet (diff, extra, seek) describing how many bytes
to copy from old + diff block, how many bytes to copy from the extra block, and
how many bytes to seek afterwards.

There is a problem with that approach: Each control entry requires 3 seeks in
3 different parts of the patch file. This is inefficient.

### ddelta patch format

The ddelta patch format consists of a header:

    char magic[8];
    uint64_t new_file_size;

followed by a stream of entries, which each consist of a header followed by
the diff data and the extra data associated with the file:

    uint64_t diff;
    uint64_t extra;
    int64_t seek;

    unsigned char diffdata[diff];
    unsigned char extradata[extra];
    
This way, we can simply stream the patch, allowing us to read it from a pipe,
for example. The patches are uncompressed, as they are intended to be stored
in an .xz compressed tarball.

The file is terminated by an entry where all header fields are 0.
