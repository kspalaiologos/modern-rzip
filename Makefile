
# Copyright (C) Kamila Szewczyk 2022

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

CC=clang
FLAGS=-g3 -O3 -march=native -mtune=native
CFLAGS=$(FLAGS)
CXXFLAGS=$(FLAGS)

ZPAQ_LIB=-Ivendor/zpaq/ vendor/zpaq/libzpaq.o
LZ4_LIB=-Ivendor/lz4/lib vendor/lz4/lib/liblz4.a
ZSTD_LIB=-Ivendor/zstd/lib vendor/zstd/lib/libzstd.a
FLZMA2_LIB=-Ivendor/fast-lzma2/ vendor/fast-lzma2/libfast-lzma2.a
BZIP3_LIB=-Ivendor/bzip3/include vendor/bzip3/src/libbz3.c
PPMDSH_VARJR1_LIB=vendor/cxx_glue.o -Ivendor/ppmd_sh/ -Ivendor/ppmd_sh/libpmd
LIBS=$(ZPAQ_LIB) $(LZ4_LIB) $(FLZMA2_LIB) $(ZSTD_LIB) $(BZIP3_LIB) $(PPMDSH_VARJR1_LIB)
SOURCES=$(wildcard src/*.c)

mrzip: $(SOURCES)
	$(CC) $(CFLAGS) -Iinclude -o $@ $^ $(LIBS) -lstdc++ -lm -pthread -lpthread -lgcrypt -lgpg-error -static

.PHONY: clean
clean:
	rm -f mrzip

vendor/cxx_glue.o: vendor/cxx_glue.cpp
	$(CXX) $(CXXFLAGS) -Ivendor/ppmd_sh/ -Ivendor/ppmd_sh/libpmd -c -o $@ $^

.PHONY: deps
deps: vendor/cxx_glue.o vendor/zpaq vendor/lz4 vendor/zstd vendor/fast-lzma2 vendor/bzip3
	$(MAKE) -C vendor/zpaq
	$(MAKE) -C vendor/lz4
	$(MAKE) -C vendor/zstd
	$(MAKE) -C vendor/fast-lzma2
	cd vendor/bzip3 && ./bootstrap.sh && ./configure && $(MAKE)
