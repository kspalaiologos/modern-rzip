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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* We can correct a ~131K block of zeroes in the middle of the file. */
#define BLK_LEN (16 * 511)

#include "blake2b.h"
#include "reed-solomon.h"

int main(int argc, char * argv[]) {
    uint8_t * ec_buf = NULL;
    uint8_t * tr_buf = NULL;
    long decode = 0;
    long i = 0;
    long sum_of_failures = 0;
    long uncorrectable_failures = 0;

    ec_buf = malloc(255 * BLK_LEN * sizeof(uint8_t));
    tr_buf = malloc(255 * BLK_LEN * sizeof(uint8_t));
    if (ec_buf == NULL || tr_buf == NULL) {
        perror("malloc");
        exit(1);
    }
    memset(ec_buf, 0, 255 * BLK_LEN * sizeof(uint8_t));
    memset(tr_buf, 0, 255 * BLK_LEN * sizeof(uint8_t));

    blake2b_state s[1];
    blake2b_init(s, BLAKE2B_OUTBYTES);

    uint8_t hash[BLAKE2B_OUTBYTES];

    if (argc == 2 && !strcmp(argv[1], "-d")) {
        decode = 1;
    }

    if (decode) {
        long got = fread(tr_buf, 1, BLK_LEN * 255, stdin);

        goto process;
        
        do {
            long got = fread(tr_buf, 1, BLK_LEN * 255, stdin);

            if(got == BLK_LEN * 255) {
                for (i = 0; i < BLK_LEN; i++) {
                    if(fwrite(ec_buf + i * 255, 1, 223, stdout) != 223) {
                        perror("fwrite");
                        exit(1);
                    }
                }
            } else if(got != BLAKE2B_OUTBYTES + 4) {
                fprintf(stderr, "rs-mrzip: file truncated. can't validate the checksum or remove superfluous 0x00 padding, but the data might be ok.\n");
                for (i = 0; i < BLK_LEN; i++) {
                    if(fwrite(ec_buf + i * 255, 1, 223, stdout) != 223) {
                        perror("fwrite");
                        exit(1);
                    }
                }
                exit(1);
            } else {
                blake2b_final(s, hash, BLAKE2B_OUTBYTES);
                if(memcmp(hash, tr_buf, BLAKE2B_OUTBYTES) != 0) {
                    fprintf(stderr, "rs-mrzip: checksum mismatch, too many errors or header corruption.\n");
                }
                unsigned char * data = tr_buf + BLAKE2B_OUTBYTES;
                uint16_t k_i = 0, k_j = 0;
                k_i = data[0]; k_i |= data[1] << 8;
                k_j = data[2]; k_j |= data[3] << 8;
                for(i = 0; i < BLK_LEN; i++) {
                    if(k_i != i) {
                        if(fwrite(ec_buf + i * 255, 1, 223, stdout) != 223) {
                            perror("fwrite");
                            exit(1);
                        }
                    } else {
                        if(fwrite(ec_buf + i * 255, 1, k_j, stdout) != k_j) {
                            perror("fwrite");
                            exit(1);
                        }
                        goto end;
                    }
                }
                goto end;
            }

process:
            distribute(tr_buf, ec_buf, BLK_LEN, 255, 1);

            int eras_pos[32] = { 0 };
            for (i = 0; i < BLK_LEN; i++) {
                int failure = rsd32((unsigned char *)(ec_buf + i * 255), eras_pos, 0);
                if (failure > 0) {
                    sum_of_failures += failure;
                } else if (failure == -1) {
                    uncorrectable_failures++;
                }
                blake2b_update(s, ec_buf + i * 255, 223);
            }
        } while(!feof(stdin));

        if (sum_of_failures > 0 || uncorrectable_failures > 0) {
            fprintf(stderr, "rs-mrzip: number of corrected errors: %ld\n", argv[0], sum_of_failures);
        }
    } else {
        uint16_t k_i = 0xFFFF, k_j = 0xFFFF;
        while (!feof(stdin)) {
            for (i = 0; i < BLK_LEN; i++) {
                long got = fread(ec_buf + i * 255, 1, 223, stdin);
                if (got < 223) {
                    memset(ec_buf + i * 255 + got, 0x00, 223 - got);
                    if(k_i == 0xFFFF && k_j == 0xFFFF) {
                        k_i = i;
                        k_j = got;
                    }
                }
                blake2b_update(s, ec_buf + i * 255, 223);
                rse32(ec_buf + i * 255, ec_buf + i * 255 + 223);
            }
            distribute(ec_buf, tr_buf, BLK_LEN, 255, 0);
            if (BLK_LEN * 255 != fwrite(tr_buf, 1, BLK_LEN * 255, stdout)) {
                perror("fwrite");
                exit(1);
            }
        }
        
        blake2b_final(s, hash, BLAKE2B_OUTBYTES);
        if (BLAKE2B_OUTBYTES != fwrite(hash, 1, BLAKE2B_OUTBYTES, stdout)) {
            perror("fwrite");
            exit(1);
        }

        putchar(k_i & 0xFF);
        putchar((k_i >> 8) & 0xFF);
        putchar(k_j & 0xFF);
        putchar((k_j >> 8) & 0xFF);
    }

end:
    free(ec_buf);
    free(tr_buf);
    
    return 0;
}
