
.PHONY: deps
deps:
	make -C vendor/zpaq
	make -C vendor/lz4
	make -C vendor/zstd
	make -C vendor/fast-lzma2
	cd vendor/bzip3 && ./configure && make
