#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <string>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

#if defined __MSVCRT__
    #include <fcntl.h>
    #include <io.h>
#endif

#include "../common/blake2b.h"
#include <filesystem>
namespace fs = std::filesystem;

using namespace std::literals::chrono_literals;

#include "tlsh.h"

class latch {
    private:
        unsigned delay;
        std::atomic<unsigned> count;
    
    public:
        latch(unsigned delay) : delay(delay), count(0) {}
        
        bool operator()() {
            count++;
            if(count == delay) {
                count = 0;
                return true;
            }
            return false;
        }
};

class blake2b_cksum {
   public:
    uint8_t digest[64];

    bool operator==(const blake2b_cksum & other) const { return memcmp(digest, other.digest, 64) == 0; }

    bool operator<(const blake2b_cksum & other) const { return memcmp(digest, other.digest, 64) < 0; }
};

// This needs some explaining. Basically, TLSH is a locality-sensitive hash that assigns a short byte
// string to a not-so-short file, and in premise, similar files will have similar hashes. We order files
// for better compression. This problem is somewhat comparable to the Travelling Salesman Problem.
// I am going to game this problem later by putting hashes in overlapping buckets per population count
// to speed up the TSP solution. For now we just use an approximation :).

class tlsh_digest {
    public:
        uint8_t digest[TLSH_STRING_BUFFER_LEN];
        
        int compare_to(const tlsh_digest & other) const {
            // Return the amount of bytes that are the same.
            int score = 0;
            for(int i = 0; i < TLSH_STRING_BUFFER_LEN; i++)
                score += digest[i] == other.digest[i];
            return score;
        }
};

class file {
   public:
    uint64_t modification_date;
    uint64_t size;
    uint64_t archive_offset;
    blake2b_cksum checksum;
    fs::path name;
    tlsh_digest digest;
    /* 88 + len(name) + 4 + TLSH_STRING_BUFFER_LEN */
};

void write_u64(uint64_t value) {
    uint8_t bytes[8];
    bytes[0] = (value >> 56) & 0xFF;
    bytes[1] = (value >> 48) & 0xFF;
    bytes[2] = (value >> 40) & 0xFF;
    bytes[3] = (value >> 32) & 0xFF;
    bytes[4] = (value >> 24) & 0xFF;
    bytes[5] = (value >> 16) & 0xFF;
    bytes[6] = (value >> 8) & 0xFF;
    bytes[7] = value & 0xFF;
    fwrite(bytes, 1, 8, stdout);
}

uint64_t read_u64() {
    uint8_t bytes[8];
    fread(bytes, 1, 8, stdin);
    return ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) | ((uint64_t)bytes[2] << 40) | ((uint64_t)bytes[3] << 32) | ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16) | ((uint64_t)bytes[6] << 8) | bytes[7];
}

void write_u32(uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    fwrite(bytes, 1, 4, stdout);
}

uint32_t read_u32() {
    uint8_t bytes[4];
    fread(bytes, 1, 4, stdin);
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3];
}

void compute_checksums(file & f, const fs::path & e) {
    Tlsh tlsh;
    blake2b_state state;
    blake2b_init(&state, 64);

    int fd = open(e.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "open(" << e.c_str() << ") failed: " << strerror(errno) << std::endl;
        exit(1);
    }
    char buffer[4096];
    ssize_t read_size;
    if(f.size > 500) {
        while ((read_size = read(fd, buffer, sizeof(buffer))) > 0) {
            tlsh.update((const unsigned char *)buffer, read_size);
            blake2b_update(&state, buffer, read_size);
        }
    } else {
        while ((read_size = read(fd, buffer, sizeof(buffer))) > 0) {
            blake2b_update(&state, buffer, read_size);
        }
    }

    if (read_size == -1) {
        std::cerr << "read failed: " << strerror(errno) << std::endl;
        exit(1);
    }
    close(fd);
    if(f.size > 500) {
        tlsh.final((const unsigned char *)buffer, read_size, 0);
        tlsh.getHash((char *)f.digest.digest, TLSH_STRING_BUFFER_LEN, 0);
    } else {
        memset(f.digest.digest, 0, TLSH_STRING_BUFFER_LEN);
    }
    blake2b_final(&state, f.checksum.digest, 64);
}

int64_t current_time_secs() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// Create an archive from directory `dir' and output it to the standard output.
void create(const char * dir) {
    std::vector<file> files;

    std::string base_dir = fs::canonical(dir);

    uint64_t total_size = 0;

    std::cerr << "Creating an archive out of " << base_dir << "." << std::endl << "* Scanning files..." << std::endl;

    latch output_latch(1000);

    for (auto & e : fs::recursive_directory_iterator(dir)) {
        if (e.is_directory()) continue;
        if (!e.is_regular_file()) {
            std::cerr << "skipping non-regular file, symlinks presently unsupported: " << e.path() << std::endl;
            continue;
        }
        file current;

        std::cerr << "\33[2K\rAdding file " << files.size() << ": " << e.path() << "..." << std::flush;

        // Set basic properties of the file.
        current.name = fs::relative(e.path(), base_dir);
        total_size += current.size = e.file_size();

        // Query the creation/modification times.
        current.modification_date = fs::last_write_time(e.path()).time_since_epoch().count();

        // Append the file.
        files.push_back(std::move(current));
    }

    std::cerr << std::endl << "* Computing checksums..." << std::endl;

    int processors = std::thread::hardware_concurrency();
    if(processors == 0) processors = 4;

    // Compute checksums in parallel displaying status every 100MB.
    {
        std::atomic_size_t checksums_done = 0, checksum_total_bytes = 0, checksum_running_bytes = 0;
        std::mutex display_mutex;

        std::vector<std::thread> threads;

        for(int i = 0; i < processors; i++) {
            threads.emplace_back([&]() {
                while(true) {
                    size_t index = checksums_done++;
                    if(index >= files.size()) break;
                    compute_checksums(files[index], base_dir / files[index].name);
                    checksum_total_bytes += files[index].size;
                    checksum_running_bytes += files[index].size;
                }
            });
        }

        std::atomic_bool stop = false;

        std::thread display = std::thread([&]() {
            while(!stop) {
                if(checksum_running_bytes > 100000000) {
                    std::cerr << "\33[2K\r" << checksum_total_bytes / 1000000 << "MB done ..." << std::flush;
                    checksum_running_bytes = 0;
                }
                std::this_thread::sleep_for(10ms);
            }
        });

        for(auto & t : threads) t.join();

        stop = true;
        display.join();
    }

    // Write the header.
    fwrite("ARZIP", 5, 1, stdout);

    // Compute the size of metadata.
    uint64_t metadata_size = 0;
    for (auto & f : files) metadata_size += f.name.string().length() + 88 + 4 + TLSH_STRING_BUFFER_LEN;
    write_u64(metadata_size);

    // Order nodes. TODO: Speedup.
    std::cerr << std::endl;
    std::cerr << "* Ordering files..." << std::endl;
    {
        std::atomic_size_t files_processed = 0;
        auto now = current_time_secs();
        uint64_t next = 0, next_score = 0, c = 0, first_node = 0, last_node = files.size();
        while(c + 1 < last_node) {
            auto elapsed = current_time_secs() - now;
            std::cerr << "\33[2K\rOrdering files " << files_processed++ << "/" << files.size() << ", " << (c / (elapsed + 1)) << " files/s..." << std::flush;

            for(uint64_t i = c + 1; i < last_node; i++) {
                int score = files[c].digest.compare_to(files[i].digest);
                if(next_score < score) { next_score = score; next = i; if(score > 130) break; }
            }

            // Swap.
            std::swap(files[c + 1], files[next]);
            next_score = next = 0;
            c++;
        }

        std::cerr << std::endl;
        std::cerr << "* Time elapsed: " << current_time_secs() - now << "s" << std::endl;
    }

    // Collapse files with the same checksum (assign the same offset).
    uint64_t files_size = 0;
    uint64_t dedup_size = 0;
    {
        std::map<blake2b_cksum, uint64_t> checksums;
        uint64_t offset = 0;
        for (auto & f : files) {
            if (checksums.find(f.checksum) == checksums.end()) {
                f.archive_offset = checksums[f.checksum] = offset;
                offset += f.size;
            } else {
                f.archive_offset = checksums[f.checksum];
                dedup_size += f.size;
            }

            files_size += f.size;

            if(output_latch()) std::cerr << "\33[2K\r" << dedup_size / 1024 << "KB / " << files_size / 1024 << "KB deduped" << std::flush;
        }
    }

    std::cerr << std::endl << "* Writing metadata (" << (metadata_size/1024) << " KB)..." << std::endl;

    // Write the metadata.
    for (auto & f : files) {
        write_u64(f.modification_date);
        write_u64(f.size);
        write_u64(f.archive_offset);
        fwrite(f.checksum.digest, 1, 64, stdout);
        fwrite(f.digest.digest, 1, TLSH_STRING_BUFFER_LEN, stdout);
        write_u32(f.name.string().length());
        fwrite(f.name.c_str(), 1, f.name.string().length(), stdout);
    }

    std::cerr << "* Writing the archive..." << std::endl;

    // Write the files. TODO: print debug info less often.
    uint64_t current_offset = 0;
    for (auto & f : files) {
        if (f.archive_offset < current_offset) {
            // File already written, ignore.
            continue;
        }
        assert(f.archive_offset == current_offset);
        if (!fs::exists(base_dir / f.name)) {
            std::cerr << "File " << f.name << " does not exist anymore, the header has been written already. Fatal error." << std::endl;
            exit(1);
        }
        if (fs::last_write_time(base_dir / f.name).time_since_epoch().count() != f.modification_date) {
            std::cerr << std::endl
                      << "warning: file " << f.name << " has been modified since the archive was created." << std::endl;
        }
        int fd = open((base_dir / f.name).c_str(), O_RDONLY);
        if (fd == -1) {
            std::cerr << "open(" << (base_dir / f.name).c_str() << ") failed: " << strerror(errno) << std::endl;
            exit(1);
        }
        char buffer[4096];
        ssize_t read_size;
        size_t bytes_left = f.size;
        while ((read_size = read(fd, buffer, std::min(sizeof(buffer), bytes_left))) > 0) {
            fwrite(buffer, 1, read_size, stdout);
            current_offset += read_size;
            bytes_left -= read_size;
        }
        if (read_size == -1) {
            std::cerr << "read failed: " << strerror(errno) << std::endl;
            exit(1);
        }
        close(fd);
        if(output_latch()) std::cerr << "\33[2K\r" << current_offset / 1024 << "KB / " << (files_size - dedup_size) / 1024 << "KB written"
                           << std::flush;
    }

    std::cerr << std::endl << "* Done." << std::endl;

    // Flush the output.
    fflush(stdout);
}

// Extract the archive from the standard input here.
void extract() {
    char header[5];
    fread(header, 5, 1, stdin);
    if (memcmp(header, "ARZIP", 5) != 0) {
        std::cerr << "Invalid header." << std::endl;
        exit(1);
    }

    // Read the metadata.
    uint64_t metadata_size = read_u64();
    std::vector<file> files;
    while (metadata_size > 0) {
        file f;
        f.modification_date = read_u64();
        f.size = read_u64();
        f.archive_offset = read_u64();
        fread(f.checksum.digest, 1, 64, stdin);
        fread(f.digest.digest, 1, TLSH_STRING_BUFFER_LEN, stdin);
        uint32_t name_length = read_u32();
        char name[name_length + 1];
        fread(name, 1, name_length, stdin);
        name[name_length] = 0;
        f.name = name;
        if(fs::path(f.name).is_absolute()) {
            std::cerr << "Absolute path in archive: " << f.name << std::endl;
            exit(1);
        }
        if(fs::path(f.name).lexically_normal() != fs::path(f.name)) {
            std::cerr << "Path not normalized: " << f.name << std::endl;
            exit(1);
        }
        files.push_back(f);
        metadata_size -= name_length + 88 + 4 + TLSH_STRING_BUFFER_LEN;
    }

    // Sort by the archive offset
    std::sort(files.begin(), files.end(), [](const file & a, const file & b) { return a.archive_offset < b.archive_offset; });

    // Extract.
    uint64_t current_offset = 0;

    for(size_t i = 0; i < files.size(); i++) {
        if(i + 1 < files.size() && files[i].archive_offset == files[i + 1].archive_offset) {
            // Two or more files point to the same place.
            size_t duplicates = 0, orig_i = i;
            do {
                i++; duplicates++;
            } while(i < files.size() && files[i].archive_offset == files[i - 1].archive_offset);
            i--;

            // Create files, update their modification dates.
            std::vector<int> fds;
            for(size_t j = 0; j < duplicates; j++) {
                fs::create_directories(fs::path(files[orig_i + j].name).parent_path());
                if(fs::exists(files[orig_i + j].name))
                    std::cerr << "File " << files[orig_i + j].name << " already exists, overwriting." << std::endl;
                int dest_fd = open(files[orig_i + j].name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(dest_fd == -1) {
                    std::cerr << "open(" << files[orig_i + j].name << ") failed: " << strerror(errno) << std::endl;
                    exit(1);
                }
                fds.push_back(dest_fd);
                fs::last_write_time(files[orig_i + j].name, fs::file_time_type(fs::file_time_type::duration(files[orig_i + j].modification_date)));
            }
            
            // Copy the data.
            char buffer[4096];
            ssize_t read_size;
            size_t bytes_left = files[orig_i].size;
            blake2b_state state;
            blake2b_init(&state, 64);
            while ((read_size = fread(buffer, 1, std::min(sizeof(buffer), bytes_left), stdin)) > 0) {
                for(int fd : fds) {
                    if(write(fd, buffer, read_size) != read_size) {
                        std::cerr << "write failed: " << strerror(errno) << std::endl;
                        exit(1);
                    }
                }
                blake2b_update(&state, buffer, read_size);
                current_offset += read_size;
                bytes_left -= read_size;
            }
            if (read_size == -1) {
                std::cerr << "fread failed: " << strerror(errno) << std::endl;
                exit(1);
            }
            for(int fd : fds) close(fd);

            char current_digest[64];
            blake2b_final(&state, current_digest, 64);

            if(memcmp(current_digest, files[orig_i].checksum.digest, 64) != 0) {
                std::cerr << "Checksum mismatch for " << files[orig_i].name << std::endl;
                exit(1);
            }
        } else {
            // Create the file, update its modification date.
            fs::create_directories(fs::path(files[i].name).parent_path());
            if(fs::exists(files[i].name))
                std::cerr << "File " << files[i].name << " already exists, overwriting." << std::endl;
            int dest_fd = open(files[i].name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(dest_fd == -1) {
                std::cerr << "open(" << files[i].name << ") failed: " << strerror(errno) << std::endl;
                exit(1);
            }
            fs::last_write_time(files[i].name, fs::file_time_type(fs::file_time_type::duration(files[i].modification_date)));

            // Copy the data.
            char buffer[4096];
            ssize_t read_size;
            size_t bytes_left = files[i].size;
            blake2b_state state;
            blake2b_init(&state, 64);
            while ((read_size = fread(buffer, 1, std::min(sizeof(buffer), bytes_left), stdin)) > 0) {
                if(write(dest_fd, buffer, read_size) != read_size) {
                    std::cerr << "write failed: " << strerror(errno) << std::endl;
                    exit(1);
                }
                blake2b_update(&state, buffer, read_size);
                current_offset += read_size;
                bytes_left -= read_size;
            }
            if (read_size == -1) {
                std::cerr << "fread failed: " << strerror(errno) << std::endl;
                exit(1);
            }
            close(dest_fd);

            char current_digest[64];
            blake2b_final(&state, current_digest, 64);

            if(memcmp(current_digest, files[i].checksum.digest, 64) != 0) {
                std::cerr << "Checksum mismatch for " << files[i].name << std::endl;
                exit(1);
            }
        }
    }
}

#include "../include/config.h"
#include <getopt.h>

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"extract", no_argument, 0, 'x'},
    {"create", required_argument, 0, 'c'},
    {"regex", required_argument, 0, 'r'},
    {"verbose", no_argument, 0, 'v'},
    {"force", no_argument, 0, 'f'},
    {"skip", no_argument, 0, 's'},
    {"dest", required_argument, 0, 'd'},
    {0, 0, 0, 0}
};

static const char * short_options = "hVxcr:fsd:";

static void usage(void) {
    std::cerr << (PACKAGE
                 " version " PACKAGE_VERSION
                 "\n"
                 "Copyright (C) Kamila Szewczyk 2022\n"
                 "Usage: ar-mrzip [options] -d < [archive] OR ar-mrzip [options] [source] > [archive]\n"
                 "General options:\n"
                 "--------------------\n"
                 "  -x, --extract          extract from the archive\n"
                 "  -c, --create           create an archive from files in directory\n"
                 "  -r, --regex            perform the operations only on files that match a regex\n"
                 "  -v, --verbose          enable verbose output for progress monitoring\n"
                 "  -h, --help             display this message\n"
                 "  -V, --version          display version information\n"
                 "  -f, --force            force overwriting of existing files\n"
                 "  -s, --skip             skip existing files\n"
                 "  -d, --dir              set the destination directory for extraction or source directory for archiving\n"
                 "\n"
                 "The archive data for extraction is read from standard input. The created archive data is written to standard output.\n");
}

static void version(void) {
    std::cerr << (PACKAGE " version " PACKAGE_VERSION
                         "\n"
                         "Copyright (C) Kamila Szewczyk 2022\n"
                         "This is free software.  You may redistribute copies of it under the terms of\n"
                         "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
                         "There is NO WARRANTY, to the extent permitted by law.\n");
}

enum { OP_EXTRACT, OP_CREATE };
enum { FILE_BEHAVIOUR_FORCE, FILE_BEHAVIOUR_SKIP, FILE_BEHAVIOUR_ASK };

int main(int argc, char * argv[]) {
    #if defined(__MSVCRT__)
        setmode(STDIN_FILENO, O_BINARY);
        setmode(STDOUT_FILENO, O_BINARY);
    #endif

    // Parse arguments using getopt_long.
    int c; std::string destdir = "."; int operation = OP_EXTRACT; std::string regex = ""; int verbose = 0; int file_behaviour = FILE_BEHAVIOUR_ASK;

    while((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch(c) {
            case 'h':
                usage();
                return 0;
            case 'V':
                version();
                return 0;
            case 'd':
                destdir = optarg;
                break;
            case 'x':
                operation = OP_EXTRACT;
                break;
            case 'c':
                operation = OP_CREATE;
                break;
            case 'r':
                regex = optarg;
                break;
            case 's':
                file_behaviour = FILE_BEHAVIOUR_SKIP;
                break;
            case 'f':
                file_behaviour = FILE_BEHAVIOUR_FORCE;
                break;
            case 'v':
                verbose = 1;
                break;
            case '?':
                return 1;
            default:
                abort();
        }
    }
    
    if(operation == OP_EXTRACT) {
        if(optind != argc) {
            std::cerr << "Too many arguments." << std::endl;
            return 1;
        }
        extract();
    } else if(operation == OP_CREATE) {
        if(optind == argc) {
            std::cerr << "No source directory specified." << std::endl;
            return 1;
        }
        if(optind + 1 != argc) {
            std::cerr << "Too many arguments." << std::endl;
            return 1;
        }
        create(argv[optind]);
    }
    
    return 0;
}
