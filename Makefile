
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

ZPAQ_LIB=-Ivendor/zpaq/ vendor/zpaq/libzpaq.o
LZ4_LIB=-Ivendor/lz4/lib vendor/lz4/lib/liblz4.a
ZSTD_LIB=-Ivendor/zstd/lib vendor/zstd/lib/libzstd.a
FLZMA2_LIB=-Ivendor/fast-lzma2/ vendor/fast-lzma2/libfast-lzma2.a
BZIP3_LIB=-Ivendor/bzip3/include vendor/bzip3/src/libbz3.c
PPMDSH_VARJR1_LIB=vendor/ppmd_sh.o -Ivendor/ppmd_sh/ -Ivendor/ppmd_sh/libpmd

mrzip:
	$(CC) $(CFLAGS) -o $@ src/*.c -Iinclude -pthread -lpthread \
		$(ZPAQ_LIB) $(LZ4_LIB) $(FLZMA2_LIB) $(ZSTD_LIB) $(BZIP3_LIB) $(PPMDSH_VARJR1_LIB) -lstdc++ -lm -static

.PHONY: deps
deps: vendor/zpaq vendor/lz4 vendor/zstd vendor/fast-lzma2 vendor/bzip3
	$(MAKE) -C vendor/zpaq
	$(MAKE) -C vendor/lz4
	$(MAKE) -C vendor/zstd
	$(MAKE) -C vendor/fast-lzma2
	cd vendor/bzip3 && ./bootstrap.sh && ./configure && $(MAKE)
	$(CXX) $(CXXFLAGS) -c -o $(PPMDSH_VARJR1_LIB) vendor/ppmd_sh.cpp
