
#include <filesystem>
#include <iostream>
#include <string>
#include <cstring>
#include <map>
#include <vector>
#include <cassert>
#include <tuple>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../common/blake2b.h"

class blake2b_cksum {
    private:
        uint8_t digest[64];
    public:
        void from(const std::filesystem::path & e) {
            int fd = open(e.c_str(), O_RDONLY);
            if(fd == -1) {
                std::cerr << "open(" << e.c_str() << ") failed: " << strerror(errno) << std::endl;
                exit(1);
            }
            blake2b_state state;
            blake2b_init(&state, 64);
            char buffer[4096];
            ssize_t read_size;
            while((read_size = read(fd, buffer, sizeof(buffer))) > 0)
                blake2b_update(&state, buffer, read_size);
            if(read_size == -1) {
                std::cerr << "read failed: " << strerror(errno) << std::endl;
                exit(1);
            }
            blake2b_final(&state, digest, 64);
            close(fd);
        }

        bool operator==(const blake2b_cksum & other) const {
            return memcmp(digest, other.digest, 64) == 0;
        }

        bool operator<(const blake2b_cksum & other) const {
            return memcmp(digest, other.digest, 64) < 0;
        }

        const uint8_t * get_digest() const {
            return digest;
        }
};

class file {
    public:
        /*  0 */ uint64_t modification_date;
        /*  8 */ uint64_t creation_date;
        /* 16 */ uint64_t size;
        /* 24 */ uint64_t archive_offset;
        /* 32 */ blake2b_cksum checksum;
        /* 96 */ std::filesystem::path name;
        /* 96 + len(name) + 4 */
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

std::pair<uint64_t, uint64_t> get_times(const std::filesystem::path & e) {
    struct statx stat;
    if (statx(AT_FDCWD, e.c_str(), AT_STATX_SYNC_AS_STAT, STATX_ALL, &stat) == -1) {
        std::cerr << "statx failed: " << strerror(errno) << std::endl;
        exit(1);
    }
    return std::make_pair(stat.stx_mtime.tv_sec, stat.stx_ctime.tv_sec);
} 

// Create an archive from directory `dir' and output it to the standard output.
void create(const char * dir) {
    std::vector<file> files;

    std::string base_dir = std::filesystem::canonical(dir);

    for(auto & e : std::filesystem::recursive_directory_iterator(dir)) {
        if(e.is_directory())
            continue;
        if(!e.is_regular_file()) {
            std::cerr << "skipping non-regular file, symlinks unsupported yet: " << e.path() << std::endl;
            continue;
        }
        file current;

        // Set basic properties of the file.
        current.name = std::filesystem::relative(e.path(), base_dir);
        current.size = e.file_size();

        // Query the creation/modification times.
        auto [modification_date, creation_date] = get_times(e.path());
        current.modification_date = modification_date;
        current.creation_date = creation_date;

        // Compute the blake2b checksum.
        current.checksum.from(e.path());

        // Append the file.
        files.push_back(std::move(current));
    }

    // Write the header.
    fwrite("ARZIP", 5, 1, stdout);

    // Compute the size of metadata.
    uint64_t metadata_size = 0;
    for(auto & f : files)
        metadata_size += f.name.string().length() + 96 + 4;
    write_u64(metadata_size);

    // Collapse files with the same checksum (assign the same offset).
    {
        std::map<blake2b_cksum, uint64_t> checksums;
        uint64_t offset = 0;
        for(auto & f : files) {
            if(checksums.find(f.checksum) == checksums.end()) {
                f.archive_offset = checksums[f.checksum] = offset;
                offset += f.size;
            } else {
                f.archive_offset = checksums[f.checksum];
            }
        }
    }

    // Write the metadata.
    for(auto & f : files) {
        write_u64(f.modification_date);
        write_u64(f.creation_date);
        write_u64(f.size);
        write_u64(f.archive_offset);
        fwrite(f.checksum.get_digest(), 1, 64, stdout);
        write_u32(f.name.string().length());
        fwrite(f.name.c_str(), 1, f.name.string().length(), stdout);
    }

    // Write the files.
    uint64_t current_offset = 0;
    for(auto & f : files) {
        if(f.archive_offset < current_offset) {
            // File already written, ignore.
            continue;
        }
        assert(f.archive_offset == current_offset);
        auto [modification_date, creation_date] = get_times(base_dir / f.name);
        if(modification_date != f.modification_date) {
            std::cerr << "warning: file " << f.name << " has been modified since the archive was created." << std::endl;
        }
        int fd = open((base_dir / f.name).c_str(), O_RDONLY);
        if(fd == -1) {
            std::cerr << "open(" << (base_dir / f.name).c_str() << ") failed: " << strerror(errno) << std::endl;
            exit(1);
        }
        char buffer[4096];
        ssize_t read_size;
        size_t bytes_left = f.size;
        while((read_size = read(fd, buffer, std::min(sizeof(buffer), bytes_left))) > 0) {
            fwrite(buffer, 1, read_size, stdout);
            current_offset += read_size;
            bytes_left -= read_size;
        }
        if(read_size == -1) {
            std::cerr << "read failed: " << strerror(errno) << std::endl;
            exit(1);
        }
        close(fd);
    }

    // Flush the output.
    fflush(stdout);
}

// Extract the archive from the standard input here.
void extract() {

}

int main(int argc, char * argv[]) {
    if(argc == 2) {
        create(argv[1]);
    } else if(argc == 1) {
        extract();
    } else return 1;
}
