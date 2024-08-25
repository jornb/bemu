#pragma once
#include <fstream>
#include <string>

#include "../types.hpp"
#include "save_state.hpp"

namespace bemu::gb {
struct FileOutputBuffer {
    std::ofstream file;

    void write(const u8 data) { file << data; }
};

struct FileInputBuffer {
    std::ifstream file;

    u8 read() {
        u8 data = 0;
        file.read(reinterpret_cast<char*>(&data), 1);
        return data;
    }
};

template <typename TEmulator>
void save_state_to_file(TEmulator& emulator, const std::string& filename) {
    std::ofstream stream{filename, std::ios::binary};
    auto buffer = FileOutputBuffer{std::move(stream)};
    auto archive = StateOutputArchive{buffer};
    emulator.serialize(archive);
}

template <typename TEmulator>
void load_state_from_file(TEmulator& emulator, const std::string& filename) {
    std::ifstream stream{filename, std::ios::binary};
    auto buffer = FileInputBuffer{std::move(stream)};
    auto archive = StateInputArchive{buffer};
    emulator.serialize(archive);
}
}  // namespace bemu::gb
