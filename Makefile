
.PHONY: deps
deps:
	cd vendor
	cd zpaq && make -C && cd ..
	cd lz4 && make -C && cd ..
	cd zstd && make -C && cd ..
	cd bzip3 && ./configure && make && cd ..
	cd fast-lzma2 && make && cd ..
