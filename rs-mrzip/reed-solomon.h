/*
   Copyright (C) 2022 Kamila Szewczyk
   Copyright (C) 1999 Phil Karn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/* The CCSDS Reed-Solomon algorithm implementation. Assumes 223-byte
   large blocks; produces 255-byte large output. Can correct at most
   16 bytes corruped anywhere in the block. Generates data in Berlekamp's
   dual-basis representation. */

#ifndef _REED_SOLOMON_H
#define _REED_SOLOMON_H

/* Simplified: We can correct a ~131K block of zeroes in the middle
   of the file. */
#define BLK_LEN (16 * 511)

/* Scatter data from src to dst. We want to distribute the Reed-Solomon
   algorithm output across the storage device, so that e.g. in case of
   a bad sector, only one byte of 512 blocks is corrupted, meaning that
   one can easily recover it in each and every of the blocks. */
void scatter(uint8_t * src, uint8_t * dst, int rows, int cols);

/* Gather data from src to dst. Undo the operation performed by
   scatter. */
void gather(uint8_t * src, uint8_t * dst, int rows, int cols);

/* Decode data. This implementation of Reed-Solomon algorithm allows
   to provide an erasure locator polynomial, which signifies the
   positions of known erasures. Presently unused. Of course,
   erasures can be treated as errors, but this takes up an extra
   code word. */
int rsd32(uint8_t data[255], int eras_pos[32], int no_eras);

/* Encode data. */
int rse32(uint8_t data[223], uint8_t bb[32]);

#endif
