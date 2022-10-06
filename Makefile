
ZPAQ_LIB=-Ivendor/zpaq/ vendor/zpaq/libzpaq.o
LZ4_LIB=-Ivendor/lz4/lib vendor/lz4/lib/liblz4.a
ZSTD_LIB=-Ivendor/zstd/lib vendor/lz4/lib/libzstd.a
FLZMA2_LIB=-Ivendor/fast-lzma2/ vendor/fast-lzma2/libfast-lzma2.a
BZIP3_LIB=-Ivendor/bzip3/include vendor/bzip3/src/libbz3.c

mrzip:
	$(CC) $(CFLAGS) -o $@ src/*.c -Iinclude -pthread -lpthread $(ZPAQ_LIB) $(LZ4_LIB) $(FLZMA2_LIB) -lm -static

.PHONY: deps
deps: vendor/zpaq vendor/lz4 vendor/zstd vendor/fast-lzma2 vendor/bzip3
	make -C vendor/zpaq
	make -C vendor/lz4
	make -C vendor/zstd
	make -C vendor/fast-lzma2
	cd vendor/bzip3 && ./bootstrap.sh && ./configure && make
