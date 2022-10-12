
# rs-mrzip

An utility bundled with mrzip to implant a recovery record into `mrzip`-compressed files, making them error-resilient.

A worked example of error recovery:

```
# Protect file `data'.
rs-mrzip < data > data.rsm

# Unpack the file before corrupting it to validate round-trip.
rs-mrzip -d < data.rsm > data.rec

# Verify correctness.
md5sum data data.rec

# Corrupt the file (overwrite first ~131K of the file with random data).
dd if=/dev/urandom of=data.rsr bs=512 count=255 conv=notrunc

# Try to recover the file after corruption.
rs-mrzip -d < data.rsr > data.rec

# Verify correctness.
md5sum data data.rec
```

## Implementation details

`rs-mrzip` uses the Reed-Solomon algorithm to create a recovery record for the compressed data. The data is padded to a multiple of the burst packet size (2 MiB). The BLAKE2b checksum is used to verify correctness of unpacked data after recovery. A contiguous, corrupted and not truncated block of up to around 131K can be fixed.
