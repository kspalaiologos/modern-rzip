
# Copyright (C) Kamila Szewczyk 2022

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either vearion 2 of the License, or
# (at your option) any later vearion.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

CONFIG=-I../vendor/filesystem/include/ -I../vendor/tlsh/include

include ../common.mk

AR_MRZIP_OBJECTS=ar-mrzip.o ../vendor/tlsh/src/tlsh.o ../vendor/tlsh/src/tlsh_util.o \
				 ../vendor/tlsh/src/tlsh_impl.o
AR_MRZIP_LIBS=../common/blake2b.o
PROGRAM=ar-mrzip

all: $(PROGRAM)

$(PROGRAM): $(AR_MRZIP_OBJECTS)
	@echo "   CCLD" $@
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(AR_MRZIP_LIBS)
	@echo "   STRIP" $@
	@$(STRIP) $(PROGRAM)

.PHONY: clean
clean:
	@rm -f $(PROGRAM) $(AR_MRZIP_OBJECTS)

.PHONY: format
format:
	clang-format -i *.cpp

.PHONY: install
install:
	../build-aux/install-sh -c $(PROGRAM) $(BIN_DEST)

.PHONY: uninstall
uninstall:
	rm -f $(BIN_DEST)/$(PROGRAM)
