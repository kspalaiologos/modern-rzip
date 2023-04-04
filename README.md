modern-rzip.
======================

A backup suite. Supports FLZMA2, PPMD, bzip3, LZ4, Zstandard, LSH i-node ordering deduplicating archiver, long range deduplication, encryption and recovery records. Also a refurbished partial rewrite of lrzip in active development.

### Download, build and install
```
% git clone --recurse-submodules https://github.com/kspalaiologos/modern-rzip
% cd modern-rzip && ./configure && make -j$(nproc) common all && sudo make install
```

### Usage
```
# Compress:
ar-mrzip -c data_dir | mrzip -L9 > data_dir.mar
# Decompress:
mkdir data_dir && mrzip -d < data_dir.mar | ar-mrzip -x data_dir
```

### How it Works
**modern-rzip** applies a two-step process and reads file or STDIN input, passes it to the **rzip**
pre-processor. The rzip pre-processor applies long-range redundancy reduction and then passes the
streams of data to a back-end compressor. **modern-rzip** will, by default, test each stream with
a *compressibility* test using **lz4** prior to compression. The selected back-end compressor works
on smaller data sets and ignore streams of data that may not compress well. The end result is
significantly faster compression than standalone compressors and much faster decompression.

**modern-rzip**'s compressors are:
* Fast LZMA2 (default)
* Zstandard
* PPMd_sh based on vJr1
* LZ4
* zpaq
* bzip3
* rzip (pre-processing only)

**modern-rzip**'s memory management scheme permits maximum use of system ram to pre-process files and then compress them.

### Thanks
Con Kolivas - the creator of `lrzip`, Peter Hyman - maintainer of the `lrzip-next` fork.
