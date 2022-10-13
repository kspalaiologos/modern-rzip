
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

include ../common.mk

COMMON_MRZIP_OBJECTS=blake2b.o

all: $(COMMON_MRZIP_OBJECTS)

.PHONY: clean
clean:
	@rm -f $(COMMON_MRZIP_OBJECTS)

.PHONY: format
format:
	clang-format -i *.c *.h
