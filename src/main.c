/*
   Copyright (C) 2006-2016, 2021 Con Kolivas
   Copyright (C) 2011, 2019, 2020, 2022 Peter Hyman
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

#include <arpa/inet.h>
#include <dirent.h>
#include <getopt.h>
#include <libgen.h>
#include <locale.h>
#include <signal.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "../include/config.h"
#include "../include/mrzip_core.h"
#include "../include/rzip.h"
#include "../include/stream.h"
#include "../include/util.h"

#ifndef PRIO_MIN
#define PRIO_MIN -20
#endif

#ifndef PRIO_MAX
#define PRIO_MAX 20
#endif

#define MAX_PATH_LEN 4096

// progress flag
bool progress_flag = false;

/* These hash and encryption constants will be referenced in the
 * control structure. */

#define MAXHASH 13
#define MAXENC 2

struct hash hashes[MAXHASH + 1] = {
    { "CRC", 0, GCRY_MD_CRC32, 4 },
    { "MD5", 1, GCRY_MD_MD5, 16 },
    { "RIPEMD", 2, GCRY_MD_RMD160, 20 },
    { "SHA256", 3, GCRY_MD_SHA256, 32 },
    { "SHA384", 4, GCRY_MD_SHA384, 48 },
    { "SHA512", 5, GCRY_MD_SHA512, 64 },
    { "SHA3_256", 6, GCRY_MD_SHA3_256, 32 },
    { "SHA3_512", 7, GCRY_MD_SHA3_512, 64 },
    { "SHAKE128_16", 8, GCRY_MD_SHAKE128, 16 },  /* XOF function */
    { "SHAKE128_32", 9, GCRY_MD_SHAKE128, 32 },  /* XOF function */
    { "SHAKE128_64", 10, GCRY_MD_SHAKE128, 64 }, /* XOF function */
    { "SHAKE256_16", 11, GCRY_MD_SHAKE256, 16 }, /* XOF function */
    { "SHAKE256_32", 12, GCRY_MD_SHAKE256, 32 }, /* XOF function */
    { "SHAKE256_64", 13, GCRY_MD_SHAKE256, 64 }, /* XOF function */
};

struct encryption encryptions[MAXENC + 1] = {
    { "NONE", 0, 0, 0, 0 }, /* encryption not used */
    { "AES128", 1, GCRY_CIPHER_AES128, 16, 16 },
    { "AES256", 2, GCRY_CIPHER_AES256, 32, 16 },
};

static rzip_control base_control, local_control, *control;

static void usage(void) {
    print_output(PACKAGE
                 " version " PACKAGE_VERSION
                 "\n"
                 "Copyright (C) Con Kolivas 2006-2021\n"
                 "Copyright (C) Peter Hyman 2007-2022\n"
                 "Copyright (C) Kamila Szewczyk 2022\n"
                 "Based on rzip "
                 "Copyright (C) Andrew Tridgell 1998-2003\n\n"
                 "Usage: mrzip [options] <file...>\n"
                 "Compression Options:\n"
                 "--------------------\n"
                 "	--lzma			lzma compression (default)\n"
                 "	-s, --ppmdsh		ppmd_sh varJr1 compression\n"
                 "	-Z, --zstd		zstd compression using zlib\n"
                 "	-l, --lz4		lz4 compression (ultra fast)\n"
                 "	-n, --no-compress	no backend compression - prepare for other compressor\n"
                 "	-z, --zpaq		zpaq compression (best, extreme compression, extremely slow)\n"
                 "	-B, --bzip3		bzip3 compression\n"
                 "	-L#, --level #		set lzma/zstd compression level (1-9, default 7)\n"
                 "	--fast			alias for -L1\n"
                 "	--best			alias for -L9\n"
                 "	--zpaqbs		Set ZPAQ Block Size overriding defaults. 1-11, 2^zpaqbs * 1MB\n"
                 "	--bzip3bs		Set bzip3 Block Size. 1-8, 2^bzip3bs * 1MB.\n"
                 "    Additional Compression Options:\n"
                 "	-C, --comment [comment]	Add a comment up to 64 chars\n"
                 "	-e, --encrypt [=password] password protected sha512/aes128 encryption on compression\n"
                 "	-E, --emethod [method]	Encryption Method: 1 = AES128, 2=AES256\n"
                 "	-D, --delete		delete existing files\n"
                 "	-f, --force		force overwrite of any existing files\n"
                 "	-K, --keep-broken	keep broken or damaged output files\n"
                 "	-o, --outfile filename	specify the output file name and/or path\n"
                 "	-O, --outdir directory	specify the output directory when -o is not used\n"
                 "	-S, --suffix suffix	specify compressed suffix (default '.lrz')\n"
                 "    Low level Compression Options:\n"
                 "	-N, --nice-level value	Set nice value to value (default 19)\n"
                 "	-m, --maxram size	Set maximum available ram in hundreds of MB\n"
                 "\t\t\t\tOverrides detected amount of available ram. Useful for testing\n"
                 "	-R, --rzip-level level	Set independent RZIP Compression Level (1-9) for pre-processing "
                 "(default=compression level)\n"
                 "	-T, --threshold [limit]	Disable LZ4 compressibility testing OR set limit to determine "
                 "compressibiity (1-99)\n"
                 "\t\t\t\tNote: Since limit is optional, the short option must not have a space. e.g. -T75, not -T 75\n"
                 "	-U, --unlimited		Use unlimited window size beyond ramsize (potentially much slower)\n"
                 "	-w, --window size	maximum compression window in hundreds of MB\n"
                 "\t\t\t\tdefault chosen by heuristic dependent on ram and chosen compression\n"
                 "Decompression Options:\n"
                 "----------------------\n"
                 "	-d, --decompress	decompress\n"
                 "	-e, -f -o -O		Same as Compression Options\n"
                 "	-t, --test		test compressed file integrity\n"
                 "	-c, --check		check integrity of file written on decompression\n"
                 "General Options:\n"
                 "----------------\n"
                 "	-h, -?, --help		show help\n"
                 "	-H, --hash [hash code]	Set hash to compute (default md5) 1-13 (see manpage)\n"
                 "	-i, --info		show compressed file information\n"
                 "	-P, --progress		show compression progress\n"
                 "	-q, --quiet		don't show compression progress\n"
                 "	-p, --threads value	Set processor count to override number of threads\n"
                 "	-v[v], --verbose	Increase verbosity\n"
                 "	-V, --version		display software version and license\n"
                 "\nMRZIP=NOCONFIG environment variable setting can be used to bypass mrzip.conf.\n"
                 "TMP environment variable will be used for storage of temporary files when needed.\n"
                 "TMPDIR may also be stored in mrzip.conf file.\n"
                 "\nIf no filenames or \"-\" is specified, stdin/out will be used.\n");
}

static void license(void) {
    print_output(PACKAGE " version " PACKAGE_VERSION
                         "\n"
                         "Copyright (C) Con Kolivas 2006-2016\n"
                         "Copyright (C) Peter Hyman 2007-2022\n"
                         "Copyright (C) Kamila Szewczyk 2022\n"
                         "Based on rzip Copyright (C) Andrew Tridgell 1998-2003\n\n"
                         "This is free software.  You may redistribute copies of it under the terms of\n"
                         "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
                         "There is NO WARRANTY, to the extent permitted by law.\n");
}

static void sighandler(int sig) {
    signal(sig, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    print_err("Interrupted\n");
    fatal_exit(&local_control);
}

static char * compression_type(void) {
    if(LZMA_COMPRESS) return "LZMA";
    else if(LZ4_COMPRESS) return "LZ4";
    else if(PPM_COMPRESS) return "PPM";
    else if(ZSTD_COMPRESS) return "ZSTD";
    else if(ZPAQ_COMPRESS) return "ZPAQ";
    else if(BZIP3_COMPRESS) return "BZIP3";
    else if(NO_COMPRESS) return "RZIP pre-processing only";
    fatal("Internal error in main.c:compression_type - unknown compression type.");
}

static void show_summary(void) {
    /* OK, if verbosity set, print summary of options selected */
    if (!INFO) {
        print_verbose("The following options are in effect for this %s.\n", DECOMPRESS  ? "DECOMPRESSION"
                                                                            : TEST_ONLY ? "INTEGRITY TEST"
                                                                                        : "COMPRESSION");
        print_verbose("Threading is %s. Number of CPUs detected: %'d\n", control->threads > 1 ? "ENABLED" : "DISABLED",
                      control->threads);
        print_verbose("Detected %'" PRId64 " bytes ram\n", control->ramsize);
        print_verbose("Nice Value: %'d\n", control->nice_val);
        print_verbose("Show Progress\n");
        print_maxverbose("Max ");
        print_verbose("Verbose\n");
        if (FORCE_REPLACE) print_verbose("Overwrite Files\n");
        if (!KEEP_FILES) print_verbose("Remove input files on completion\n");
        if (control->outdir)
            print_verbose("Output Directory Specified: %s\n", control->outdir);
        else if (control->outname)
            print_verbose("Output Filename Specified: %s\n", control->outname);
        if (TEST_ONLY) print_verbose("Test file integrity\n");
        if (control->tmpdir) print_verbose("Temporary Directory set as: %s\n", control->tmpdir);

        /* show compression options */
        if (!DECOMPRESS && !TEST_ONLY) {
            print_verbose(
                "Compression mode is: %s", compression_type());
            if (!LZ4_COMPRESS && !ZSTD_COMPRESS)
                print_verbose(". LZ4 Compressibility testing %s\n", (LZ4_TEST ? "enabled" : "disabled"));
            if (LZ4_TEST && control->threshold != 100) print_verbose("Threshhold limit = %'d%%\n", control->threshold);
            print_verbose("Compression level %'d\n", control->compression_level);
            print_verbose("RZIP Compression level %'d\n", control->rzip_compression_level);
            if (ZPAQ_COMPRESS)
                print_verbose("ZPAQ Compression Level: %'d, ZPAQ initial Block Size: %'d\n", control->zpaq_level,
                              control->zpaq_bs);
            if (BZIP3_COMPRESS) print_verbose("BZIP3 Compression Block Size: %'d\n", control->bzip3_bs);
            print_verbose("%s Hashing Used\n", control->hash_label);
            if (ENCRYPT) print_verbose("%s Encryption Used\n", control->enc_label);
            if (control->window)
                print_verbose("Compression Window: %'" PRId64 " = %'" PRId64 "MB\n", control->window,
                              control->window * 100ull);
            /* show heuristically computed window size */
            if (!control->window && !UNLIMITED) {
                i64 temp_chunk, temp_window;

                if (STDOUT || STDIN)
                    temp_chunk = control->maxram;
                else
                    temp_chunk = control->ramsize * 2 / 3;
                temp_window = temp_chunk / (100 * ONE_MB);
                print_verbose("Heuristically Computed Compression Window: %'" PRId64 " = %'" PRId64 "MB\n", temp_window,
                              temp_window * 100ull);
            }
            if (UNLIMITED) print_verbose("Using Unlimited Window size\n");
            print_maxverbose("Storage time in seconds %'" PRId64 "\n", control->secs);
        }
    }
}

static struct option long_options[] = {
    { "ppmdsh", no_argument, 0, 's' }, /* 0 */
    { "check", no_argument, 0, 'c' },
    { "comment", required_argument, 0, 'C' },
    { "decompress", no_argument, 0, 'd' },
    { "delete", no_argument, 0, 'D' },
    { "encrypt", optional_argument, 0, 'e' }, /* 5 */
    { "emethod", required_argument, 0, 'E' },
    { "force", no_argument, 0, 'f' },
    { "zstd", no_argument, 0, 'Z' },
    { "help", no_argument, 0, 'h' },
    { "hash", optional_argument, 0, 'H' }, /* 10 */
    { "info", no_argument, 0, 'i' },
    { "keep-broken", no_argument, 0, 'K' },
    { "lz4", no_argument, 0, 'l' },
    { "level", optional_argument, 0, 'L' },
    { "maxram", required_argument, 0, 'm' }, /* 15 */
    { "no-compress", no_argument, 0, 'n' },
    { "nice-level", required_argument, 0, 'N' },
    { "outfile", required_argument, 0, 'o' },
    { "outdir", required_argument, 0, 'O' },
    { "threads", required_argument, 0, 'p' }, /* 20 */
    { "progress", no_argument, 0, 'P' },
    { "quiet", no_argument, 0, 'q' },
    { "rzip-level", required_argument, 0, 'R' },
    { "suffix", required_argument, 0, 'S' },
    { "test", no_argument, 0, 't' }, /* 25 */
    { "threshold", optional_argument, 0, 'T' },
    { "unlimited", no_argument, 0, 'U' },
    { "verbose", no_argument, 0, 'v' },
    { "version", no_argument, 0, 'V' },
    { "window", required_argument, 0, 'w' }, /* 30 */
    { "zpaq", no_argument, 0, 'z' },
    { "bzip3", no_argument, 0, 'B' },
    { "fast", no_argument, 0, '1' },
    { "best", no_argument, 0, '9' },
    { "lzma", no_argument, 0, 0 }, /* 34 - begin long opt index */
    { "zpaqbs", required_argument, 0, 0 },
    { "bzip3bs", required_argument, 0, 0 },
    { 0, 0, 0, 0 },
};

/* constants for ease of maintenance in getopt loop */
#define LONGSTART 34

static void set_stdout(struct rzip_control * control) {
    control->flags |= FLAG_STDOUT;
    control->outFILE = stdout;
    control->msgout = stderr;
    register_outputfile(control, control->msgout);
}

static const char * loptions = "BcC:dDe::E:fhH::iKlL:nN:o:O:p:PqR:sS:tT::Um:vVw:zZ?";

int main(int argc, char * argv[]) {
    bool lrzncat = false;
    bool options_file = false,
         conf_file_compression_set = false; /* for environment and tracking of compression setting */
    struct timeval start_time, end_time;
    struct sigaction handler;
    double seconds, total_time;  // for timers
    bool nice_set = false;
    int c, i, ds, long_opt_index;
    int hours, minutes;
    extern int optind;
    char *eptr, *av; /* for environment */
    char * endptr = NULL;
    char * pwd = NULL;

    control = &base_control;

    initialise_control(control);

    av = basename(argv[0]);
    if (!strcmp(av, "lrznunzip"))
        control->flags |= FLAG_DECOMPRESS;
    else if (!strcmp(av, "lrzncat")) {
        control->flags |= FLAG_DECOMPRESS | FLAG_STDOUT;
        lrzncat = true;
    }

    /* Get Preloaded Defaults from mrzip.conf
     * Look in ., $HOME/.mrzip/, /etc/mrzip.
     * If MRZIP=NOCONFIG is set, then ignore config
     * If mrzip.conf sets a compression mode, options_file will be true.
     * This will allow for a test to permit an override of compression mode.
     * If there is an override, then all compression settings will be reset
     * and command line switches will prevail, including for --lzma.
     */
    eptr = getenv("MRZIP");
    if (eptr == NULL)
        options_file = read_config(control);
    else if (!strstr(eptr, "NOCONFIG"))
        options_file = read_config(control);
    if (options_file && (control->flags & FLAG_NOT_LZMA)) /* if some compression set in mrzip.conf    */
        conf_file_compression_set = true;                 /* need this to allow command line override */

    pwd = getenv("PWD"); /* get PWD for output directory test */

    setlocale(LC_NUMERIC, control->locale); /* for printf features. Defailt is current locale */

    while ((c = getopt_long(argc, argv, loptions, long_options, &long_opt_index)) != -1) {
        switch (c) {
            case 's':
            case 'Z':
            case 'l':
            case 'n':
            case 'z':
            case 'B':
                /* If some compression was chosen in mrzip.conf, allow this one time
                 * because conf_file_compression_set will be true
                 */
                if ((control->flags & FLAG_NOT_LZMA) && conf_file_compression_set == false)
                    fatal("Can only use one of -l, -b, -g, -z, -B or -n\n");
                /* Select Compression Mode */
                control->flags &= ~FLAG_NOT_LZMA; /* must clear all compressions first */
                if (c == 's')
                    control->flags |= FLAG_PPM_COMPRESS;
                else if (c == 'Z')
                    control->flags |= FLAG_ZSTD_COMPRESS;
                else if (c == 'l')
                    control->flags |= FLAG_LZ4_COMPRESS;
                else if (c == 'n')
                    control->flags |= FLAG_NO_COMPRESS;
                else if (c == 'z')
                    control->flags |= FLAG_ZPAQ_COMPRESS;
                else if (c == 'B')
                    control->flags |= FLAG_BZIP3_COMPRESS;
                /* now FLAG_NOT_LZMA will evaluate as true */
                conf_file_compression_set = false;
                break;
            case 'c':
                control->flags |= FLAG_CHECK;
                control->flags |= FLAG_HASH;
                break;
            case 'C': /* get comment if any */
                if (!optarg) fatal("Must provide a comment\n");
                control->comment_length = strlen(optarg);
                if (control->comment_length > 64)
                    fatal("Comment string too long, %d bytes.\n", control->comment_length);

                if (control->comment_length) control->comment = strdup(optarg);
                break;
            case 'd':
                control->flags |= FLAG_DECOMPRESS;
                break;
            case 'D':
                control->flags &= ~FLAG_KEEP_FILES;
                break;
            case 'e':
                control->flags |= FLAG_ENCRYPT;
                if (optarg) control->passphrase = strdup(optarg);
                break;
            case 'E':  // Encryption code
                if (!optarg) fatal("Enryption method must be declared.\n");
                i = strtol(optarg, &endptr, 10);
                if (i < 1 || i > MAXENC) fatal("Encryption method must be 1 or 2 for AES 128 or 256\n");
                control->enc_code = i;
                break;
            case 'f':
                control->flags |= FLAG_FORCE_REPLACE;
                break;
            case 'h':
            case '?':
                usage();
                exit(0);
            case 'H':
                control->flags |= FLAG_HASHED;
                if (optarg) {
                    i = strtol(optarg, &endptr, 10);
                    if (*endptr) fatal("Extra characters after Hash: \'%s\'\n", endptr);
                    if (i < 1 || i > MAXHASH) fatal("Hash codes out of bounds. Must be between 1 and %d.\n", MAXHASH);
                    control->hash_code = i;
                }
                break;
            case 'i':
                control->flags |= FLAG_INFO;
                control->flags &= ~FLAG_DECOMPRESS;
                break;
            case 'K':
                control->flags |= FLAG_KEEP_BROKEN;
                break;
            case '1': /* fast and best level options */
            case '9':
                control->compression_level = c - '0';
                break;
            case 'L':
                control->compression_level = strtol(optarg, &endptr, 10);
                if (*endptr) fatal("Extra characters after compression level: \'%s\'\n", endptr);
                if (control->compression_level < 1 || control->compression_level > 9)
                    fatal("Invalid compression level (must be 1-9)\n");
                break;
            case 'R':
                /* explicitly set rzip compression level */
                control->rzip_compression_level = strtol(optarg, &endptr, 10);
                if (*endptr) fatal("Extra characters after rzip compression level: \'%s\'\n", endptr);
                if (control->rzip_compression_level < 1 || control->rzip_compression_level > 9)
                    fatal("Invalid rzip compression level (must be 1-9)\n");
                break;
            case 'm':
                control->ramsize = strtol(optarg, &endptr, 10) * ONE_MB * 100;
                if (*endptr) fatal("Extra characters after ramsize: \'%s\'\n", endptr);
                break;
            case 'N':
                nice_set = true;
                control->nice_val = strtol(optarg, &endptr, 10);
                if (*endptr) fatal("Extra characters after nice level: \'%s\'\n", endptr);
                if (control->nice_val < PRIO_MIN || control->nice_val > PRIO_MAX)
                    fatal("Invalid nice value (must be %'d...%'d)\n", PRIO_MIN, PRIO_MAX);
                break;
            case 'o':
                if (control->outdir) fatal("Cannot have -o and -O together\n");
                if (unlikely(STDOUT)) fatal("Cannot specify an output filename when outputting to stdout\n");
                control->outname = strdup(optarg);
                break;
            case 'O':
                if (control->outname) /* can't mix -o and -O */
                    fatal("Cannot have options -o and -O together\n");
                if (unlikely(STDOUT)) fatal("Cannot specify an output directory when outputting to stdout\n");
                control->outdir = malloc(strlen(optarg) + 2);
                if (control->outdir == NULL) fatal("Failed to allocate for outdir\n");
                strcpy(control->outdir, optarg);
                if (strcmp(optarg + strlen(optarg) - 1, "/")) /* need a trailing slash */
                    strcat(control->outdir, "/");
                break;
            case 'p':
                control->threads = strtol(optarg, &endptr, 10);
                if (*endptr) fatal("Extra characters after number of threads: \'%s\'\n", endptr);
                if (control->threads < 1) fatal("Must have at least one thread\n");
                break;
            case 'P':
                control->flags |= FLAG_SHOW_PROGRESS;
                break;
            case 'q':
                control->flags &= ~FLAG_SHOW_PROGRESS;
                break;
            case 'S':
                if (control->outname) fatal("Specified output filename already, can't specify an extension.\n");
                if (unlikely(STDOUT)) fatal("Cannot specify a filename suffix when outputting to stdout\n");
                control->suffix = strdup(optarg);
                break;
            case 't':
                if (control->outname) fatal("Cannot specify an output file name when just testing.\n");
                if (!KEEP_FILES) fatal("Doubt that you want to delete a file when just testing.\n");
                control->flags |= FLAG_TEST_ONLY;
                break;
            case 'T':
                /* process optional threshold limit
                 * or disable threshold testing
                 */
                if (optarg) {
                    i = strtol(optarg, &endptr, 10);
                    if (*endptr) fatal("Extra characters after threshold limit: \'%s\'\n", endptr);
                    if (i < 1 || i > 99) fatal("Threshhold limits are 1-99\n");
                    control->threshold = i;
                } else
                    control->flags &= ~FLAG_THRESHOLD;
                break;
            case 'U':
                control->flags |= FLAG_UNLIMITED;
                break;
            case 'v':
                /* set progress flag */
                if (!(control->flags & FLAG_SHOW_PROGRESS)) control->flags |= FLAG_SHOW_PROGRESS;
                /* set verbosity flag */
                if (!(control->flags & FLAG_VERBOSITY) && !(control->flags & FLAG_VERBOSITY_MAX))
                    control->flags |= FLAG_VERBOSITY;
                /* or set max verbosity flag */
                else if ((control->flags & FLAG_VERBOSITY)) {
                    control->flags &= ~FLAG_VERBOSITY;
                    control->flags |= FLAG_VERBOSITY_MAX;
                }
                break;
            case 'V':
                license();
                exit(0);
                break;
            case 'w':
                control->window = strtol(optarg, &endptr, 10);
                if (*endptr) fatal("Extra characters after window size: \'%s\'\n", endptr);
                if (control->window < 1) fatal("Window must be positive\n");
                break;
            case 0: /* these are long options without a short code */
                switch (long_opt_index) {
                    /* in case lzma selected, need to reset not lzma flag */
                    case LONGSTART:
                        control->flags &= ~FLAG_NOT_LZMA; /* clear alternate compression flags */
                        break;
                    case LONGSTART + 1:
                        if (!ZPAQ_COMPRESS)
                            print_err("--zpaqbs option only valid for ZPAQ compression. Ignored.\n");
                        else {
                            ds = strtol(optarg, &endptr, 10);
                            if (*endptr) fatal("Extra characters after block size: \'%s\'\n", endptr);
                            if (ds < 0 || ds > 11) fatal("ZPAQ Block Size must be between 1 and 11\n");
                            control->zpaq_bs = ds;
                        }
                        break;
                    case LONGSTART + 2:
                        if (!BZIP3_COMPRESS)
                            print_err("--bzip3bs option only valid for BZIP3 compression. Ignored.\n");
                        else {
                            ds = strtol(optarg, &endptr, 10);
                            if (*endptr) fatal("Extra characters after block size: \'%s\'\n", endptr);
                            if (ds < 0 || ds > 8) fatal("BZIP3 Block Size must be between 1 and 8\n");
                            control->bzip3_bs = ds;
                        }
                        break;
                }       // switch
                break;  // break out of longopt switch
            default:    // oops
                usage();
                exit(1);

        }  // main switch
    }      // while input loop

    argc -= optind;
    argv += optind;

    /* if rzip compression level not set, make equal to compression level */
    if (!control->rzip_compression_level) control->rzip_compression_level = control->compression_level;

    if (control->outname) {
        if (argc > 1) fatal("Cannot specify output filename with more than 1 file\n");
    }
    /* set suffix to default .lrz IF outname not defined */
    if (!control->suffix && !control->outname) control->suffix = strdup(".lrz");
    /* IF hash method not defined, make it MD5 */
    if (!control->hash_code) control->hash_code = 1;
    /* IF using encryption and encryption method not defined, make it AES128 */
    if (ENCRYPT && !control->enc_code) control->enc_code = 1;

    /* now set hash, crc, or encryption pointers
     * codes are either set in initialise function or
     * by command line above */
    control->crc_label = &hashes[0].label[0];
    control->crc_gcode = &hashes[0].gcode;
    control->crc_len = &hashes[0].length;

    if (!DECOMPRESS && !INFO) { /* only applies to compression */
        control->hash_label = &hashes[control->hash_code].label[0];
        control->hash_gcode = &hashes[control->hash_code].gcode;
        control->hash_len = &hashes[control->hash_code].length;

        control->enc_label = &encryptions[control->enc_code].label[0];
        control->enc_gcode = &encryptions[control->enc_code].gcode;
        control->enc_keylen = &encryptions[control->enc_code].keylen;
        control->enc_ivlen = &encryptions[control->enc_code].ivlen;
    }

    if (VERBOSE && !SHOW_PROGRESS) {
        print_err("Cannot have -v and -q options. -v wins.\n");
        control->flags |= FLAG_SHOW_PROGRESS;
    }

    if (UNLIMITED && control->window) {
        print_err("If -U used, cannot specify a window size with -w.\n");
        control->window = 0;
    }

    if (argc < 1) control->flags |= FLAG_STDIN;

    if (UNLIMITED && STDIN) {
        print_err("Cannot have -U and stdin, unlimited mode disabled.\n");
        control->flags &= ~FLAG_UNLIMITED;
    }

    /* if any filter used, disable LZ4 testing or certain compression modes */
    if ((control->flags & FLAG_THRESHOLD) && (ZSTD_COMPRESS || LZ4_COMPRESS || NO_COMPRESS)) {
        print_output("LZ4 Threshold testing disabled due to Filtering and/or Compression type (zstd, lz4, rzip).\n");
        control->flags &= ~FLAG_THRESHOLD;
    }

    setup_overhead(control);

    /* Set the main nice value to half that of the backend threads since
     * the rzip stage is usually the rate limiting step */
    control->current_priority = getpriority(PRIO_PROCESS, 0);
    if (nice_set) {
        if (!NO_COMPRESS) {
            /* If niceness can't be set. just reset process priority */
            if (unlikely(setpriority(PRIO_PROCESS, 0, control->nice_val / 2) == -1)) {
                print_err("Warning, unable to set nice value %'d...Resetting to %'d\n", control->nice_val,
                          control->current_priority);
                setpriority(PRIO_PROCESS, 0, (control->nice_val = control->current_priority));
            }
        } else {
            if (unlikely(setpriority(PRIO_PROCESS, 0, control->nice_val) == -1)) {
                print_err("Warning, unable to set nice value %'d...Resetting to %'d\n", control->nice_val,
                          control->current_priority);
                setpriority(PRIO_PROCESS, 0, (control->nice_val = control->current_priority));
            }
        }
    }

    /* One extra iteration for the case of no parameters means we will default to stdin/out */
    for (i = 0; i <= argc; i++) {
        char * infile = NULL;

        if (i < argc)
            infile = argv[i];
        else if (!(i == 0 && STDIN))
            break;
        if (infile && !(DECOMPRESS || INFO || TEST_ONLY)) {
            /* check that input file exists, unless Decompressing or Test */
            if ((strcmp(infile, "-") == 0))
                control->flags |= FLAG_STDIN;
            else {
                bool isdir = false;
                struct stat istat;

                if (unlikely(stat(infile, &istat))) fatal("Failed to stat %s\n", infile);
                isdir = S_ISDIR(istat.st_mode);
                if (isdir || !S_ISREG(istat.st_mode)) fatal("mrzip only works directly on regular FILES.\n");
            }
        }

        if (INFO && STDIN) fatal("Will not get file info from STDIN\n");

        control->infile = infile;

        /* If no output filename is specified, and we're using
         * stdin, use stdout */
        if ((control->outname && (strcmp(control->outname, "-") == 0)) || (!control->outname && STDIN) || lrzncat)
            set_stdout(control);

        if (lrzncat) {
            control->msgout = stderr;
            control->outFILE = stdout;
            register_outputfile(control, control->msgout);
        }

        if (!STDOUT) {
            control->msgout = stdout;
            register_outputfile(control, control->msgout);
        }

        if (STDIN) control->inFILE = stdin;

        /* Implement signal handler only once flags are set */
        sigemptyset(&handler.sa_mask);
        handler.sa_flags = 0;
        handler.sa_handler = &sighandler;
        sigaction(SIGTERM, &handler, 0);
        sigaction(SIGINT, &handler, 0);

        if (!FORCE_REPLACE) {
            if (STDIN && isatty(fileno((FILE *)stdin))) {
                print_err("Will not read stdin from a terminal. Use -f to override.\n");
                usage();
                exit(1);
            }
            if (!TEST_ONLY && STDOUT && isatty(fileno((FILE *)stdout))) {
                print_err("Will not write stdout to a terminal. Use -f to override.\n");
                usage();
                exit(1);
            }
        }

        if (CHECK_FILE) {
            if (!DECOMPRESS) {
                print_err("Can only check file written on decompression.\n");
                control->flags &= ~FLAG_CHECK;
            } else if (STDOUT) {
                print_err("Can't check file written when writing to stdout. Checking disabled.\n");
                control->flags &= ~FLAG_CHECK;
            }
        }

        setup_ram(control);
        show_summary();

        gettimeofday(&start_time, NULL);

        if (unlikely(STDIN && ENCRYPT && control->passphrase == NULL))
            fatal("Unable to work from STDIN while reading password. Use -e passphrase.\n");
        if (unlikely(STDOUT && !(DECOMPRESS || INFO || TEST_ONLY) && ENCRYPT))
            fatal("Unable to encrypt while writing to STDOUT.\n");

        memcpy(&local_control, &base_control, sizeof(rzip_control));
        if (DECOMPRESS || TEST_ONLY) {
            if (unlikely(!decompress_file(&local_control))) return -1;
        } else if (INFO) {
            if (unlikely(!get_fileinfo(&local_control))) return -1;
        } else if (unlikely(!compress_file(&local_control)))
            return -1;

        /* compute total time */
        gettimeofday(&end_time, NULL);
        total_time = (end_time.tv_sec + (double)end_time.tv_usec / 1000000) -
                     (start_time.tv_sec + (double)start_time.tv_usec / 1000000);
        hours = (int)total_time / 3600;
        minutes = (int)(total_time / 60) % 60;
        seconds = total_time - hours * 3600 - minutes * 60;
        if (!INFO) print_progress("Total time: %02d:%02d:%05.2f\n", hours, minutes, seconds);
    }

    return 0;
}
