/*
   Copyright (C) 2006-2016 Con Kolivas
   Copyright (C) 2011 Peter Hyman
   Copyright (C) 1998-2003 Andrew Tridgell
   Copyright (C) 2022 Kamila Szewczyk

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

#include "zpaq/libzpaq.h"

#include <stdio.h>
#ifndef uchar
#define uchar unsigned char
#endif
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define __maybe_unused	__attribute__((unused))

typedef int64_t i64;

struct bufRead: public libzpaq::Reader {
	uchar *s_buf;
	i64 *s_len;
	i64 total_len;
	int *last_pct;
	bool progress;
	int thread;
	FILE *msgout;

	bufRead(uchar *buf_, i64 *n_, i64 total_len_, int *last_pct_, bool progress_, int thread_, FILE *msgout_):
		s_buf(buf_), s_len(n_), total_len(total_len_), last_pct(last_pct_), progress(progress_), thread(thread_), msgout(msgout_) {}

	int get() {
		if (progress && !(*s_len % 128)) {
			int pct = (total_len > 0) ?
				(total_len - *s_len) * 100 / total_len : 100;

			if (pct / 10 != *last_pct / 10) {
				int i;

				fprintf(msgout, "\r\t\t\t\tZPAQ\t");
				for (i = 0; i < thread; i++)
					fprintf(msgout, "\t");
				fprintf(msgout, "%d:%i%%  \r",
					thread + 1, pct);
				fflush(msgout);
				*last_pct = pct;
			}
		}

		if (likely(*s_len > 0)) {
			(*s_len)--;
			return ((int)(uchar)*s_buf++);
		}
		return -1;
	} // read and return byte 0..255, or -1 at EOF

	int read(char *buf, int n) {
		if (unlikely(n > *s_len))
			n = *s_len;

		if (likely(n > 0)) {
			*s_len -= n;
			memcpy(buf, s_buf, n);
		}
		return n;
	}
};

struct bufWrite: public libzpaq::Writer {
	uchar *c_buf;
	i64 *c_len;
	bufWrite(uchar *buf_, i64 *n_): c_buf(buf_), c_len(n_) {}

	void put(int c) {
		c_buf[(*c_len)++] = (uchar)c;
	}

	void write(const char *buf, int n) {
		memcpy(c_buf + *c_len, buf, n);
		*c_len += n;
	}
};

extern "C" void zpaq_compress(uchar *c_buf, i64 *c_len, uchar *s_buf, i64 s_len,
		uchar *method, FILE *msgout, bool progress, int thread)
{
	i64 total_len = s_len;
	int last_pct = 100;

	bufRead bufR(s_buf, &s_len, total_len, &last_pct, progress, thread, msgout);
	bufWrite bufW(c_buf, c_len);

	compress (&bufR, &bufW, (const char *) method,  NULL, NULL, true);
}

extern "C" void zpaq_decompress(uchar *s_buf, i64 *d_len, uchar *c_buf, i64 c_len,
				FILE *msgout, bool progress, int thread)
{
	i64 total_len = c_len;
	int last_pct = 100;

	bufRead bufR(c_buf, &c_len, total_len, &last_pct, progress, thread, msgout);
	bufWrite bufW(s_buf, d_len);

	decompress(&bufR, &bufW);
}

void libzpaq::error(const char* msg) {  // print message and exit
    fprintf(stderr, "ZPAQ Error: %s\n", msg);
    exit(1);
}

#include "common.inc"
#include "coro3b_fake.inc"
#include "libpmd.inc"

int g_getc( FILE* f, FILE* g ) { return -1; }
void g_putc( int c, FILE* f, FILE* g ) { }

extern "C" {
    int ppmdsh_varjr1_compress(char * input, int input_len, char * output, int * output_size) {
        ALIGN(4096) pmd_codec C;
        uint pmd_args[] = { 12 /* Order */, 256 /* Memory */, 1 /* Restore */, 0 };
        if(C.Init(0, pmd_args))
            return -1;
        C.f = C.g = NULL;
        C.addinp((byte *)input, input_len); 
        C.addout((byte *)output, *output_size);
        while(1) {
            uint l, r = C.coro_call(&C);
            if( r==1 ) {
                C.f_quit=1;
            } else {
                *output_size = C.getoutsize();
                if(r == 2) return -1;
                else break;
            }
        }
        C.Quit();
        return 0;
    }

    int ppmdsh_varjr1_decompress(char * input, int input_len, char * output, int output_size) {
        ALIGN(4096) pmd_codec C;
        uint pmd_args[] = { 12 /* Order */, 256 /* Memory */, 1 /* Restore */, 0 };
        if(C.Init(1, pmd_args))
            return -1;
        C.f = C.g = NULL;
        C.addinp((byte *)input, input_len); 
        C.addout((byte *)output, output_size);
        while(1) {
            uint l, r = C.coro_call(&C);
            if( r==1 ) {
                C.f_quit=1;
            } else {
                if(r == 2) return -1;
                else break;
            }
        }
        C.Quit();
        return 0;
    }
}
