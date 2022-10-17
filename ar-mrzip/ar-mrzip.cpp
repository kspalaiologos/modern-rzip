#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <algorithm>
#include <atomic>
#include <string>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

#include "../common/blake2b.h"
#include <filesystem>
namespace fs = std::filesystem;

#include "tlsh.h"

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

    uint64_t total_size = 0;

    std::cerr << "Creating an archive out of " << base_dir << "." << std::endl << "* Scanning files..." << std::endl;

    for (auto & e : fs::recursive_directory_iterator(dir)) {
        if (e.is_directory()) continue;
        if (!e.is_regular_file()) {
            std::cerr << "skipping non-regular file, symlinks presently unsupported: " << e.path() << std::endl;
            continue;
        }
        file current;

        std::cerr << "\33[2K\rAdding file " << files.size() << "..." << std::flush;

        // Set basic properties of the file.
        current.name = fs::relative(e.path(), base_dir);
        total_size += current.size = e.file_size();

        // Query the creation/modification times.
        current.modification_date = fs::last_write_time(e.path()).time_since_epoch().count();

        // Append the file.
        files.push_back(std::move(current));
    }

    std::cerr << std::endl << "* Computing checksums..." << std::endl;

    // Compute checksums in parallel displaying status every 100MB.
    {
        std::atomic_size_t checksums_done = 0, checksum_total_bytes = 0, checksum_running_bytes = 0;
        std::mutex display_mutex;

        int processors = std::thread::hardware_concurrency();
        if(processors == 0) processors = 4;
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

    // Order nodes.
    std::cerr << "* Ordering files..." << std::endl;
    {
        // Start from node 0.
        uint64_t next = 0, next_score = 0, c = 0;
        while(c + 1 < files.size()) {
            std::cerr << "\33[2K\r" << c << " / " << files.size() << std::flush;

            // Find the next node.
            for(uint64_t i = c + 1; i < files.size(); i++) {
                int score = files[c].digest.compare_to(files[i].digest);
                if(score > next_score) {
                    next = i;
                    next_score = score;
                    // Good enough:
                    if(next_score >= 60) break;
                }
            }

            // Swap.
            std::swap(files[c + 1], files[next]);
            next_score = next = 0;
            c++;
        }
    }
    std::cerr << std::endl;

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
