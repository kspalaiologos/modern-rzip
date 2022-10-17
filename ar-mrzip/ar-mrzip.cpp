#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <ghc/filesystem.hpp>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "../common/blake2b.h"
namespace fs = ghc::filesystem;

#include "tlsh.h"

class blake2b_cksum {
   public:
    uint8_t digest[64];

    bool operator==(const blake2b_cksum & other) const { return memcmp(digest, other.digest, 64) == 0; }

    bool operator<(const blake2b_cksum & other) const { return memcmp(digest, other.digest, 64) < 0; }
};

class tlsh_digest {
    public:
        uint8_t digest[TLSH_STRING_BUFFER_LEN];

        int compare_to(const tlsh_digest & other) const {
            // Return the amount of bytes that are the same.
            int score = 0;
            for(int i = 0; i < TLSH_STRING_BUFFER_LEN; i++)
                if(digest[i] == other.digest[i])
                    score++;
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

void write_u32(uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    fwrite(bytes, 1, 4, stdout);
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

// Create an archive from directory `dir' and output it to the standard output.
void create(const char * dir) {
    std::vector<file> files;

    std::string base_dir = fs::canonical(dir);

    std::cerr << "Creating an archive out of " << base_dir << "." << std::endl << "* Scanning files..." << std::endl;

    for (auto & e : fs::recursive_directory_iterator(dir)) {
        if (e.is_directory()) continue;
        if (!e.is_regular_file()) {
            std::cerr << "skipping non-regular file, symlinks presently unsupported: " << e.path() << std::endl;
            continue;
        }
        file current;

        // Set basic properties of the file.
        current.name = fs::relative(e.path(), base_dir);
        current.size = e.file_size();

        // Query the creation/modification times.
        current.modification_date = fs::last_write_time(e.path()).time_since_epoch().count();

        // Compute the blake2b checksum & TLSH digest.
        compute_checksums(current, e.path());

        // Append the file.
        files.push_back(std::move(current));
    }

    // Write the header.
    fwrite("ARZIP", 5, 1, stdout);

    // Compute the size of metadata.
    uint64_t metadata_size = 0;
    for (auto & f : files) metadata_size += f.name.string().length() + 88 + 4 + TLSH_STRING_BUFFER_LEN;
    write_u64(metadata_size);

    // Order nodes.
    std::cerr << "* Ordering files..." << std::endl;
    std::sort(files.begin(), files.end(), [](const file & a, const file & b) { return a.digest < b.digest; });

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

            std::cerr << "\33[2K\r" << dedup_size / 1024 << "KB / " << files_size / 1024 << "KB deduped" << std::flush;
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

    // Write the files.
    uint64_t current_offset = 0;
    for (auto & f : files) {
        if (f.archive_offset < current_offset) {
            // File already written, ignore.
            continue;
        }
        assert(f.archive_offset == current_offset);
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
        std::cerr << "\33[2K\r" << current_offset / 1024 << "KB / " << (files_size - dedup_size) / 1024 << "KB written"
                  << std::flush;
    }

    std::cerr << std::endl << "* Done." << std::endl;

    // Flush the output.
    fflush(stdout);
}

// Extract the archive from the standard input here.
void extract() {}

int main(int argc, char * argv[]) {
    if (argc == 2) {
        create(argv[1]);
    } else if (argc == 1) {
        extract();
    } else
        return 1;
}
