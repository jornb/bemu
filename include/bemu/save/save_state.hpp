#pragma once
#include <span>
#include <type_traits>
#include <vector>

#include "../gb/emulator.hpp"

namespace bemu::gb {
template <typename Buffer>
struct StateOutputArchive {
    explicit StateOutputArchive(Buffer& buffer) : m_buffer(buffer) {}

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void operator()(const T& value) {
        const auto data = reinterpret_cast<const u8*>(&value);
        for (size_t i = 0; i < sizeof(T); ++i) {
            m_buffer.write(data[i]);
        }
    }

    template <typename T>
    void operator()(std::span<T> value) {
        for (const auto& v : value) {
            this->operator()(v);
        }
    }

    template <typename T>
    void operator()(const std::vector<T>& value) {
        this->operator()(value.size());
        for (const auto& v : value) {
            this->operator()(v);
        }
    }

   private:
    Buffer& m_buffer;
};

template <typename Buffer>
struct StateInputArchive {
    explicit StateInputArchive(Buffer& buffer) : m_buffer(buffer) {}

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void operator()(T& value) {
        auto data = reinterpret_cast<u8*>(&value);
        for (size_t i = 0; i < sizeof(T); ++i) {
            data[i] = m_buffer.read();
        }
    }

    template <typename T>
    void operator()(std::span<T> value) {
        for (auto& v : value) {
            this->operator()(v);
        }
    }

    template <typename T>
    void operator()(std::vector<T>& value) {
        size_t s = 0;
        this->operator()(s);
        value.resize(s);

        for (auto& v : value) {
            this->operator()(v);
        }
    }

   private:
    Buffer& m_buffer;
};
}  // namespace bemu::gb
