/*
   Copyright (C) 2006-2011 Con Kolivas
   Copyright (C) 2011 Peter Hyman
   Copyright (C) 1998-2003 Andrew Tridgell

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#include <sys/statvfs.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <arpa/inet.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/time.h>
#include <math.h>

#include "md5.h"
#include "rzip.h"
#include "runzip.h"
#include "util.h"
#include "stream.h"
#include "liblrzip.h" /* flag defines */

#define MAGIC_LEN (39)

/* Determine how many times to hash the password when encrypting, based on
 * the date such that we increase the number of loops according to Moore's
 * law relative to when the data is encrypted. It is then stored as a two
 * byte value in the header */
#define MOORE 1.835          // world constant  [TIMES per YEAR]
#define ARBITRARY  1000000   // number of sha2 calls per one second in 2011
#define T_ZERO 1293840000    // seconds since epoch in 2011

#define SECONDS_IN_A_YEAR (365*86400)
#define MOORE_TIMES_PER_SECOND pow (MOORE, 1.0 / SECONDS_IN_A_YEAR)
#define ARBITRARY_AT_EPOCH (ARBITRARY * pow (MOORE_TIMES_PER_SECOND, -T_ZERO))

static i64 nloops(i64 seconds, uchar *b1, uchar *b2)
{
	i64 nloops_encoded, nloops;
	int nbits;

	nloops = ARBITRARY_AT_EPOCH * pow(MOORE_TIMES_PER_SECOND, seconds);
	nbits = log (nloops) / M_LN2;
	*b1 = nbits - 7;
	*b2 = nloops >> *b1;
	nloops_encoded = (i64)*b2 << (i64)*b1;
	return nloops_encoded;
}

static i64 enc_loops(uchar b1, uchar b2)
{
	return (i64)b2 << (i64)b1;
}

static char *make_magic(rzip_control *control)
{
	struct timeval tv;
	char *magic;

	magic = calloc(MAGIC_LEN, 1);
	if (unlikely(!magic))
		fatal("Failed to calloc magic in make_magic\n");
	strcpy(magic, "LRZI");
	magic[4] = LRZIP_MAJOR_VERSION;
	magic[5] = LRZIP_MINOR_VERSION;

	/* File size is stored as zero for streaming STDOUT blocks when the
	 * file size is unknown. */
	if (!STDIN || !STDOUT || control->eof)
		memcpy(&magic[6], &control->st_size, 8);

	/* save LZMA compression flags */
	if (LZMA_COMPRESS) {
		int i;

		for (i = 0; i < 5; i++)
			magic[i + 16] = (char)control->lzma_properties[i];
	}

	/* This is a flag that the archive contains an md5 sum at the end
	 * which can be used as an integrity check instead of crc check.
	 * crc is still stored for compatibility with 0.5 versions.
	 */
	magic[21] = 1;
	if (control->encrypt)
		magic[22] = 1;

	if (unlikely(gettimeofday(&tv, NULL)))
		fatal("Failed to gettimeofday in write_magic\n");
	control->secs = tv.tv_sec;
	control->usecs = tv.tv_usec;
	memcpy(&magic[23], &control->secs, 8);
	memcpy(&magic[31], &control->usecs, 8);

	return magic;
}

void write_stdout_header(rzip_control *control)
{
	char *magic = make_magic(control);

	memcpy(control->tmp_outbuf, magic, MAGIC_LEN);
	control->magic_written = 1;

	free(magic);
}

static void write_magic(rzip_control *control, int fd_in, int fd_out)
{
	char *magic = make_magic(control);

	if (unlikely(lseek(fd_out, 0, SEEK_SET)))
		fatal("Failed to seek to BOF to write Magic Header\n");

	if (unlikely(write(fd_out, magic, MAGIC_LEN) != MAGIC_LEN))
		fatal("Failed to write magic header\n");
	control->magic_written = 1;

	free(magic);
}

void read_magic(rzip_control *control, int fd_in, i64 *expected_size)
{
	char magic[MAGIC_LEN];
	uint32_t v;
	int md5, i;

	memset(magic, 0, sizeof(magic));
	/* Initially read only <v0.6x header */
	if (unlikely(read(fd_in, magic, 24) != 24))
		fatal("Failed to read magic header\n");

	*expected_size = 0;

	if (unlikely(strncmp(magic, "LRZI", 4)))
		failure("Not an lrzip file\n");

	memcpy(&control->major_version, &magic[4], 1);
	memcpy(&control->minor_version, &magic[5], 1);

	/* Support the convoluted way we described size in versions < 0.40 */
	if (control->major_version == 0 && control->minor_version < 4) {
		memcpy(&v, &magic[6], 4);
		*expected_size = ntohl(v);
		memcpy(&v, &magic[10], 4);
		*expected_size |= ((i64)ntohl(v)) << 32;
	} else {
		memcpy(expected_size, &magic[6], 8);
		if (control->major_version == 0 && control->minor_version > 5) {
			if (unlikely(read(fd_in, magic + 24, 15) != 15))
				fatal("Failed to read magic header\n");
			if (magic[22] == 1)
				control->encrypt = 1;
			memcpy(&control->secs, &magic[23], 8);
			memcpy(&control->usecs, &magic[31], 8);
			print_maxverbose("Seconds %lld\n", control->secs);
		}
	}

	/* restore LZMA compression flags only if stored */
	if ((int) magic[16]) {
		for (i = 0; i < 5; i++)
			control->lzma_properties[i] = magic[i + 16];
	}

	/* Whether this archive contains md5 data at the end or not */
	md5 = magic[21];
	if (md5 == 1)
		control->flags |= FLAG_MD5;

	print_verbose("Detected lrzip version %d.%d file.\n", control->major_version, control->minor_version);
	if (control->major_version > LRZIP_MAJOR_VERSION ||
	    (control->major_version == LRZIP_MAJOR_VERSION && control->minor_version > LRZIP_MINOR_VERSION))
		print_output("Attempting to work with file produced by newer lrzip version %d.%d file.\n", control->major_version, control->minor_version);
}

/* preserve ownership and permissions where possible */
void preserve_perms(rzip_control *control, int fd_in, int fd_out)
{
	struct stat st;

	if (unlikely(fstat(fd_in, &st)))
		fatal("Failed to fstat input file\n");
	if (unlikely(fchmod(fd_out, (st.st_mode & 0777))))
		print_err("Warning, unable to set permissions on %s\n", control->outfile);

	/* chown fail is not fatal */
	if (unlikely(fchown(fd_out, st.st_uid, st.st_gid)))
		print_err("Warning, unable to set owner on %s\n", control->outfile);
}

/* Open a temporary outputfile to emulate stdout */
int open_tmpoutfile(rzip_control *control)
{
	int fd_out;

	if (STDOUT && !TEST_ONLY)
		print_verbose("Outputting to stdout.\n");
	if (control->tmpdir) {
		control->outfile = realloc(NULL, strlen(control->tmpdir) + 16);
		if (unlikely(!control->outfile))
			fatal("Failed to allocate outfile name\n");
		strcpy(control->outfile, control->tmpdir);
		strcat(control->outfile, "lrzipout.XXXXXX");
	} else {
		control->outfile = realloc(NULL, 16);
		if (unlikely(!control->outfile))
			fatal("Failed to allocate outfile name\n");
		strcpy(control->outfile, "lrzipout.XXXXXX");
	}

	fd_out = mkstemp(control->outfile);
	if (unlikely(fd_out == -1))
		fatal("Failed to create out tmpfile: %s\n", control->outfile);
	register_outfile(control->outfile, TEST_ONLY || STDOUT || !KEEP_BROKEN);
	return fd_out;
}

static void fwrite_stdout(void *buf, i64 len)
{
	uchar *offset_buf = buf;
	ssize_t ret;
	i64 total;

	total = 0;
	while (len > 0) {
		if (len > one_g)
			ret = one_g;
		else
			ret = len;
		ret = fwrite(offset_buf, 1, ret, stdout);
		if (unlikely(ret <= 0))
			fatal("Failed to fwrite in fwrite_stdout\n");
		len -= ret;
		offset_buf += ret;
		total += ret;
	}
	fflush(stdout);
}

void flush_stdout(rzip_control *control)
{
	print_verbose("Dumping buffer to stdout.\n");
	fwrite_stdout(control->tmp_outbuf, control->out_len);
	control->rel_ofs += control->out_len;
	control->out_ofs = control->out_len = 0;
}

/* Dump temporary outputfile to perform stdout */
void dump_tmpoutfile(rzip_control *control, int fd_out)
{
	FILE *tmpoutfp;
	int tmpchar;

	/* flush anything not yet in the temporary file */
	fsync(fd_out);
	tmpoutfp = fdopen(fd_out, "r");
	if (unlikely(tmpoutfp == NULL))
		fatal("Failed to fdopen out tmpfile\n");
	rewind(tmpoutfp);

	if (!TEST_ONLY) {
		print_verbose("Dumping temporary file to stdout.\n");
		while ((tmpchar = fgetc(tmpoutfp)) != EOF)
			putchar(tmpchar);
		fflush(stdout);
		rewind(tmpoutfp);
	}

	if (unlikely(ftruncate(fd_out, 0)))
		fatal("Failed to ftruncate fd_out in dump_tmpoutfile\n");
}

/* Open a temporary inputfile to perform stdin decompression */
int open_tmpinfile(rzip_control *control)
{
	int fd_in;

	if (control->tmpdir) {
		control->infile = malloc(strlen(control->tmpdir) + 15);
		if (unlikely(!control->infile))
			fatal("Failed to allocate infile name\n");
		strcpy(control->infile, control->tmpdir);
		strcat(control->infile, "lrzipin.XXXXXX");
	} else {
		control->infile = malloc(15);
		if (unlikely(!control->infile))
			fatal("Failed to allocate infile name\n");
		strcpy(control->infile, "lrzipin.XXXXXX");
	}

	fd_in = mkstemp(control->infile);
	if (unlikely(fd_in == -1))
		fatal("Failed to create in tmpfile: %s\n", control->infile);
	register_infile(control->infile, (DECOMPRESS || TEST_ONLY) && STDIN);
	/* Unlink temporary file immediately to minimise chance of files left
	 * lying around in cases of failure. */
	if (unlikely(unlink(control->infile)))
		fatal("Failed to unlink tmpfile: %s\n", control->infile);
	return fd_in;
}

/* Read data from stdin into temporary inputfile */
void read_tmpinfile(rzip_control *control, int fd_in)
{
	FILE *tmpinfp;
	int tmpchar;

	if (control->flags & FLAG_SHOW_PROGRESS)
		fprintf(control->msgout, "Copying from stdin.\n");
	tmpinfp = fdopen(fd_in, "w+");
	if (unlikely(tmpinfp == NULL))
		fatal("Failed to fdopen in tmpfile\n");

	while ((tmpchar = getchar()) != EOF)
		fputc(tmpchar, tmpinfp);

	fflush(tmpinfp);
	rewind(tmpinfp);
}

/*
  decompress one file from the command line
*/
void decompress_file(rzip_control *control)
{
	char *tmp, *tmpoutfile, *infilecopy = NULL;
	int fd_in, fd_out = -1, fd_hist = -1;
	i64 expected_size, free_space;
	struct statvfs fbuf;

	if (!STDIN) {
		if ((tmp = strrchr(control->infile, '.')) && strcmp(tmp,control->suffix)) {
			/* make sure infile has an extension. If not, add it
			  * because manipulations may be made to input filename, set local ptr
			*/
			infilecopy = malloc(strlen(control->infile) + strlen(control->suffix) + 1);
			if (unlikely(infilecopy == NULL))
				fatal("Failed to allocate memory for infile suffix\n");
			else {
				strcpy(infilecopy, control->infile);
				strcat(infilecopy, control->suffix);
			}
		} else
			infilecopy = strdup(control->infile);
		/* regardless, infilecopy has the input filename */
	}

	if (!STDOUT && !TEST_ONLY) {
		/* if output name already set, use it */
		if (control->outname) {
			control->outfile = strdup(control->outname);
		} else {
			/* default output name from infilecopy
			 * test if outdir specified. If so, strip path from filename of
			 * infilecopy, then remove suffix.
			*/
			if (control->outdir && (tmp = strrchr(infilecopy, '/')))
				tmpoutfile = strdup(tmp + 1);
			else
				tmpoutfile = strdup(infilecopy);

			/* remove suffix to make outfile name */
			if ((tmp = strrchr(tmpoutfile, '.')) && !strcmp(tmp, control->suffix))
				*tmp='\0';

			control->outfile = malloc((control->outdir == NULL? 0: strlen(control->outdir)) + strlen(tmpoutfile) + 1);
			if (unlikely(!control->outfile))
				fatal("Failed to allocate outfile name\n");

			if (control->outdir) {	/* prepend control->outdir */
				strcpy(control->outfile, control->outdir);
				strcat(control->outfile, tmpoutfile);
			} else
				strcpy(control->outfile, tmpoutfile);
			free(tmpoutfile);
		}

		if (!STDOUT)
			print_progress("Output filename is: %s\n", control->outfile);
	}

	if (STDIN) {
		fd_in = open_tmpinfile(control);
		read_tmpinfile(control, fd_in);
	} else {
		fd_in = open(infilecopy, O_RDONLY);
		if (unlikely(fd_in == -1)) {
			fatal("Failed to open %s\n", infilecopy);
		}
	}

	if (!(TEST_ONLY | STDOUT)) {
		if (FORCE_REPLACE)
			fd_out = open(control->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		else
			fd_out = open(control->outfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
		if (unlikely(fd_out == -1)) {
			/* We must ensure we don't delete a file that already
			 * exists just because we tried to create a new one */
			control->flags |= FLAG_KEEP_BROKEN;
			fatal("Failed to create %s\n", control->outfile);
		}

		preserve_perms(control, fd_in, fd_out);
	} else
		fd_out = open_tmpoutfile(control);
	control->fd_out = fd_out;

        read_magic(control, fd_in, &expected_size);

        if (!STDOUT) {
                /* Check if there's enough free space on the device chosen to fit the
                * decompressed file. */
                if (unlikely(fstatvfs(fd_out, &fbuf)))
                        fatal("Failed to fstatvfs in decompress_file\n");
                free_space = fbuf.f_bsize * fbuf.f_bavail;
                if (free_space < expected_size) {
                        if (FORCE_REPLACE)
                                print_err("Warning, inadequate free space detected, but attempting to decompress due to -f option being used.\n");
                        else
                                failure("Inadequate free space to decompress file, use -f to override.\n");
                }
        }

	fd_hist = open(control->outfile, O_RDONLY);
	if (unlikely(fd_hist == -1))
		fatal("Failed to open history file %s\n", control->outfile);

	/* Unlink temporary file as soon as possible */
	if (unlikely((STDOUT || TEST_ONLY) && unlink(control->outfile)))
		fatal("Failed to unlink tmpfile: %s\n", control->outfile);



	if (NO_MD5)
		print_verbose("Not performing MD5 hash check\n");
	if (HAS_MD5)
		print_verbose("MD5 ");
	else
		print_verbose("CRC32 ");
	print_verbose("being used for integrity testing.\n");

	print_progress("Decompressing...\n");

	runzip_fd(control, fd_in, fd_out, fd_hist, expected_size);

	if (STDOUT)
		dump_tmpoutfile(control, fd_out);

	/* if we get here, no fatal errors during decompression */
	print_progress("\r");
	if (!(STDOUT | TEST_ONLY))
		print_output("Output filename is: %s: ", control->outfile);
        print_progress("[OK] - %lld bytes                                \n", expected_size);

	if (unlikely(close(fd_hist) || close(fd_out)))
		fatal("Failed to close files\n");

	close(fd_in);

	if (!KEEP_FILES) {
		if (unlikely(unlink(control->infile)))
			fatal("Failed to unlink %s\n", infilecopy);
	}

	free(control->outfile);
	free(infilecopy);
}

void get_header_info(rzip_control *control, int fd_in, uchar *ctype, i64 *c_len, i64 *u_len, i64 *last_head)
{
	if (unlikely(read(fd_in, ctype, 1) != 1))
		fatal("Failed to read in get_header_info\n");

	if (control->major_version == 0 && control->minor_version < 4) {
		u32 c_len32, u_len32, last_head32;

		if (unlikely(read(fd_in, &c_len32, 4) != 4))
			fatal("Failed to read in get_header_info");
		if (unlikely(read(fd_in, &u_len32, 4) != 4))
			fatal("Failed to read in get_header_info");
		if (unlikely(read(fd_in, &last_head32, 4) != 4))
			fatal("Failed to read in get_header_info");
		*c_len = c_len32;
		*u_len = u_len32;
		*last_head = last_head32;
	} else {
		if (unlikely(read(fd_in, c_len, 8) != 8))
			fatal("Failed to read in get_header_info");
		if (unlikely(read(fd_in, u_len, 8) != 8))
			fatal("Failed to read in get_header_info");
		if (unlikely(read(fd_in, last_head, 8) != 8))
			fatal("Failed to read_i64 in get_header_info");
	}
}

void get_fileinfo(rzip_control *control)
{
	i64 u_len, c_len, last_head, utotal = 0, ctotal = 0, ofs = 49, stream_head[2];
	i64 expected_size, infile_size, chunk_size, chunk_total = 0;
	int header_length = 25, stream = 0, chunk = 0;
	char *tmp, *infilecopy = NULL;
	int seekspot, fd_in;
	char chunk_byte = 0;
	long double cratio;
	uchar ctype = 0;
	struct stat st;

	if (!STDIN) {
		if ((tmp = strrchr(control->infile, '.')) && strcmp(tmp,control->suffix)) {
			infilecopy = malloc(strlen(control->infile) + strlen(control->suffix) + 1);
			if (unlikely(infilecopy == NULL))
				fatal("Failed to allocate memory for infile suffix\n");
			else {
				strcpy(infilecopy, control->infile);
				strcat(infilecopy, control->suffix);
			}
		} else
			infilecopy = strdup(control->infile);
	}

	if (STDIN)
		fd_in = 0;
	else {
		fd_in = open(infilecopy, O_RDONLY);
		if (unlikely(fd_in == -1))
			fatal("Failed to open %s\n", infilecopy);
	}

	/* Get file size */
	if (unlikely(fstat(fd_in, &st)))
		fatal("bad magic file descriptor!?\n");
	memcpy(&infile_size, &st.st_size, 8);

	/* Get decompressed size */
	read_magic(control, fd_in, &expected_size);

	if (control->major_version == 0 && control->minor_version > 4) {
		if (unlikely(read(fd_in, &chunk_byte, 1) != 1))
			fatal("Failed to read chunk_byte in get_fileinfo\n");
		if (control->major_version == 0 && control->minor_version > 5) {
			if (unlikely(read(fd_in, &control->eof, 1) != 1))
				fatal("Failed to read eof in get_fileinfo\n");
			if (unlikely(read(fd_in, &chunk_size, 8) != 8))
				fatal("Failed to read chunk_size in get_fileinfo\n");
		}
	}

	/* Version < 0.4 had different file format */
	if (control->major_version == 0 && control->minor_version < 4)
		seekspot = 50;
	else if (control->major_version == 0 && control->minor_version == 4)
		seekspot = 74;
	else if (control->major_version == 0 && control->minor_version == 5)
		seekspot = 75;
	else
		seekspot = 99;
	if (unlikely(lseek(fd_in, seekspot, SEEK_SET) == -1))
		fatal("Failed to lseek in get_fileinfo\n");

	/* Read the compression type of the first block. It's possible that
	   not all blocks are compressed so this may not be accurate.
	 */
	if (unlikely(read(fd_in, &ctype, 1) != 1))
		fatal("Failed to read in get_fileinfo\n");

	if (control->major_version == 0 && control->minor_version < 4) {
		ofs = 24;
		header_length = 13;
	}
	if (control->major_version == 0 && control->minor_version == 4)
		ofs = 24;
	if (control->major_version == 0 && control->minor_version == 5)
		ofs = 25;
next_chunk:
	stream = 0;
	stream_head[0] = 0;
	stream_head[1] = stream_head[0] + header_length;

	print_verbose("Rzip chunk %d:\n", ++chunk);
	if (chunk_byte)
		print_verbose("Chunk byte width: %d\n", chunk_byte);
	if (chunk_size) {
		chunk_total += chunk_size;
		print_verbose("Chunk size: %lld\n", chunk_size);
	}
	while (stream < NUM_STREAMS) {
		int block = 1;

		if (unlikely(lseek(fd_in, stream_head[stream] + ofs, SEEK_SET)) == -1)
			fatal("Failed to seek to header data in get_fileinfo\n");
		get_header_info(control, fd_in, &ctype, &c_len, &u_len, &last_head);

		print_verbose("Stream: %d\n", stream);
		print_maxverbose("Offset: %lld\n", ofs);
		print_verbose("Block\tComp\tPercent\tSize\n");
		do {
			i64 head_off;

			if (unlikely(last_head + ofs > infile_size))
				failure("Offset greater than archive size, likely corrupted/truncated archive.\n");
			if (unlikely(head_off = lseek(fd_in, last_head + ofs, SEEK_SET)) == -1)
				fatal("Failed to seek to header data in get_fileinfo\n");
			get_header_info(control, fd_in, &ctype, &c_len, &u_len, &last_head);
			if (unlikely(last_head < 0 || c_len < 0 || u_len < 0))
				failure("Entry negative, likely corrupted archive.\n");
			print_verbose("%d\t", block);
			if (ctype == CTYPE_NONE)
				print_verbose("none");
			else if (ctype == CTYPE_BZIP2)
				print_verbose("bzip2");
			else if (ctype == CTYPE_LZO)
				print_verbose("lzo");
			else if (ctype == CTYPE_LZMA)
				print_verbose("lzma");
			else if (ctype == CTYPE_GZIP)
				print_verbose("gzip");
			else if (ctype == CTYPE_ZPAQ)
				print_verbose("zpaq");
			else
				print_verbose("Dunno wtf");
			utotal += u_len;
			ctotal += c_len;
			print_verbose("\t%.1f%%\t%lld / %lld", (double)c_len / (double)(u_len / 100), c_len, u_len);
			print_maxverbose("\tOffset: %lld\tHead: %lld", head_off, last_head);
			print_verbose("\n");
			block++;
		} while (last_head);
		++stream;
	}
	if (unlikely((ofs = lseek(fd_in, c_len, SEEK_CUR)) == -1))
		fatal("Failed to lseek c_len in get_fileinfo\n");
	/* Chunk byte entry */
	if (control->major_version == 0 && control->minor_version > 4) {
		if (unlikely(read(fd_in, &chunk_byte, 1) != 1))
			fatal("Failed to read chunk_byte in get_fileinfo\n");
		ofs++;
		if (control->major_version == 0 && control->minor_version > 5) {
			if (unlikely(read(fd_in, &control->eof, 1) != 1))
				fatal("Failed to read eof in get_fileinfo\n");
			if (unlikely(read(fd_in, &chunk_size, 8) != 8))
				fatal("Failed to read chunk_size in get_fileinfo\n");
			ofs += 9;
		}
	}
	if (ofs < infile_size - (HAS_MD5 ? MD5_DIGEST_SIZE : 0))
		goto next_chunk;
	if (unlikely(ofs > infile_size))
		failure("Offset greater than archive size, likely corrupted/truncated archive.\n");
	if (chunk_total > expected_size)
		expected_size = chunk_total;
	print_verbose("Rzip compression: %.1f%% %lld / %lld\n",
			(double)utotal / (double)(expected_size / 100),
			utotal, expected_size);
	print_verbose("Back end compression: %.1f%% %lld / %lld\n",
			(double)ctotal / (double)(utotal / 100),
			ctotal, utotal);
	print_verbose("Overall compression: %.1f%% %lld / %lld\n",
			(double)ctotal / (double)(expected_size / 100),
			ctotal, expected_size);

	cratio = (long double)expected_size / (long double)infile_size;

	print_output("%s:\nlrzip version: %d.%d file\n", infilecopy, control->major_version, control->minor_version);
	if (control->secs)
		print_maxverbose("Storage time seconds: %lld\n", control->secs);

	print_output("Compression: ");
	if (ctype == CTYPE_NONE)
		print_output("rzip alone\n");
	else if (ctype == CTYPE_BZIP2)
		print_output("rzip + bzip2\n");
	else if (ctype == CTYPE_LZO)
		print_output("rzip + lzo\n");
	else if (ctype == CTYPE_LZMA)
		print_output("rzip + lzma\n");
	else if (ctype == CTYPE_GZIP)
		print_output("rzip + gzip\n");
	else if (ctype == CTYPE_ZPAQ)
		print_output("rzip + zpaq\n");
	else
		print_output("Dunno wtf\n");
	print_output("Decompressed file size: %llu\n", expected_size);
	print_output("Compressed file size: %llu\n", infile_size);
	print_output("Compression ratio: %.3Lf\n", cratio);

	if (HAS_MD5) {
		char md5_stored[MD5_DIGEST_SIZE];
		int i;

		print_output("MD5 used for integrity testing\n");
		if (unlikely(lseek(fd_in, -MD5_DIGEST_SIZE, SEEK_END)) == -1)
			fatal("Failed to seek to md5 data in runzip_fd\n");
		if (unlikely(read(fd_in, md5_stored, MD5_DIGEST_SIZE) != MD5_DIGEST_SIZE))
			fatal("Failed to read md5 data in runzip_fd\n");
		print_output("MD5: ");
		for (i = 0; i < MD5_DIGEST_SIZE; i++)
			print_output("%02x", md5_stored[i] & 0xFF);
		print_output("\n");
	} else
		print_output("CRC32 used for integrity testing\n");
	if (unlikely(close(fd_in)))
		fatal("Failed to close fd_in in get_fileinfo\n");

	free(control->outfile);
	free(infilecopy);
}

/* To perform STDOUT, we allocate a proportion of ram that is then used as
 * a pseudo-temporary file */
static void open_tmpoutbuf(rzip_control *control)
{
	control->flags |= FLAG_TMP_OUTBUF;
	control->out_maxlen = control->maxram + control->page_size;
	control->tmp_outbuf = malloc(control->out_maxlen);
	if (unlikely(!control->tmp_outbuf))
		fatal("Failed to malloc tmp_outbuf in open_tmpoutbuf\n");
	control->out_ofs = control->out_len = MAGIC_LEN;
}

/*
  compress one file from the command line
*/
void compress_file(rzip_control *control)
{
	const char *tmp, *tmpinfile; 	/* we're just using this as a proxy for control->infile.
					 * Spares a compiler warning
					 */
	int fd_in, fd_out;
	char header[MAGIC_LEN];

	memset(header, 0, sizeof(header));

	if (!STDIN) {
		/* is extension at end of infile? */
		if ((tmp = strrchr(control->infile, '.')) && !strcmp(tmp, control->suffix)) {
			print_err("%s: already has %s suffix. Skipping...\n", control->infile, control->suffix);
			return;
		}

		fd_in = open(control->infile, O_RDONLY);
		if (unlikely(fd_in == -1))
			fatal("Failed to open %s\n", control->infile);
	} else
		fd_in = 0;

	if (!STDOUT) {
		if (control->outname) {
				/* check if outname has control->suffix */
				if (*(control->suffix) == '\0') /* suffix is empty string */
					control->outfile = strdup(control->outname);
				else if ((tmp=strrchr(control->outname, '.')) && strcmp(tmp, control->suffix)) {
					control->outfile = malloc(strlen(control->outname) + strlen(control->suffix) + 1);
					if (unlikely(!control->outfile))
						fatal("Failed to allocate outfile name\n");
					strcpy(control->outfile, control->outname);
					strcat(control->outfile, control->suffix);
					print_output("Suffix added to %s.\nFull pathname is: %s\n", control->outname, control->outfile);
				} else	/* no, already has suffix */
					control->outfile = strdup(control->outname);
		} else {
			/* default output name from control->infile
			 * test if outdir specified. If so, strip path from filename of
			 * control->infile
			*/
			if (control->outdir && (tmp = strrchr(control->infile, '/')))
				tmpinfile = tmp + 1;
			else
				tmpinfile = control->infile;

			control->outfile = malloc((control->outdir == NULL? 0: strlen(control->outdir)) + strlen(tmpinfile) + strlen(control->suffix) + 1);
			if (unlikely(!control->outfile))
				fatal("Failed to allocate outfile name\n");

			if (control->outdir) {	/* prepend control->outdir */
				strcpy(control->outfile, control->outdir);
				strcat(control->outfile, tmpinfile);
			} else
				strcpy(control->outfile, tmpinfile);
			strcat(control->outfile, control->suffix);
			print_progress("Output filename is: %s\n", control->outfile);
		}

		if (FORCE_REPLACE)
			fd_out = open(control->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		else
			fd_out = open(control->outfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
		if (unlikely(fd_out == -1)) {
			/* We must ensure we don't delete a file that already
			 * exists just because we tried to create a new one */
			control->flags |= FLAG_KEEP_BROKEN;
			fatal("Failed to create %s\n", control->outfile);
		}
		control->fd_out = fd_out;
		preserve_perms(control, fd_in, fd_out);
	} else 
		open_tmpoutbuf(control);

	/* Write zeroes to header at beginning of file */
	if (unlikely(!STDOUT && write(fd_out, header, sizeof(header)) != sizeof(header)))
		fatal("Cannot write file header\n");

	rzip_fd(control, fd_in, fd_out);

	/* Wwrite magic at end b/c lzma does not tell us properties until it is done */
	if (!STDOUT)
		write_magic(control, fd_in, fd_out);

	if (unlikely(close(fd_in)))
		fatal("Failed to close fd_in\n");
	if (unlikely(!STDOUT && close(fd_out)))
		fatal("Failed to close fd_out\n");
	if (TMP_OUTBUF)
		free(control->tmp_outbuf);

	if (!KEEP_FILES) {
		if (unlikely(unlink(control->infile)))
			fatal("Failed to unlink %s\n", control->infile);
	}

	free(control->outfile);
}